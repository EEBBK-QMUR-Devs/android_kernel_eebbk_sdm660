/* ktz8864 backlight controller initialization driver (v13)
 * EBBK H7000 / BBK S5: PWM dimming via PM660L pwm, I2C initializes chip only.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>

#define KTZ8864_REG_REV		0x01
#define KTZ8864_REG_BL_CFG1	0x02
#define KTZ8864_REG_BL_CFG2	0x03
#define KTZ8864_REG_BRT_LSB	0x04
#define KTZ8864_REG_BRT_MSB	0x05
#define KTZ8864_REG_BL_EN	0x08

struct ktz8864 {
	struct i2c_client *client;
	struct gpio_desc *en_gpio;
};

static int ktz8864_reg_write(struct ktz8864 *ktz, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(ktz->client, reg, val);
}

static void ktz8864_iic_init(struct ktz8864 *ktz)
{
	struct device *dev = &ktz->client->dev;
	int rev = i2c_smbus_read_byte_data(ktz->client, KTZ8864_REG_REV);
	if (rev < 0)
		dev_warn(dev, "i2c read invalid rc=%d (chip may be powered later)\n", rev);
	else
		dev_info(dev, "REV=0x%02x\n", rev);
	ktz8864_reg_write(ktz, KTZ8864_REG_BL_CFG1, 0x21);  /* PWM_ENABLE */
	ktz8864_reg_write(ktz, KTZ8864_REG_BL_CFG2, 0x80);  /* 1MHz */
	ktz8864_reg_write(ktz, KTZ8864_REG_BRT_LSB, 0x07); /* 8bit dim */
	ktz8864_reg_write(ktz, KTZ8864_REG_BRT_MSB, 0xff); /* full */
	ktz8864_reg_write(ktz, KTZ8864_REG_BL_EN, 0x9f);   /* BL_ENLED1-4 */
}

static int ktz8864_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ktz8864 *ktz;
	int rc;

	ktz = devm_kzalloc(dev, sizeof(*ktz), GFP_KERNEL);
	if (!ktz)
		return -ENOMEM;
	ktz->client = client;

	ktz->en_gpio = devm_gpiod_get_optional(dev, "ktz8864-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ktz->en_gpio))
		return PTR_ERR(ktz->en_gpio);
	if (ktz->en_gpio) {
		gpiod_set_value_cansleep(ktz->en_gpio, 1);
		usleep_range(500, 1000);
	}
	ktz8864_iic_init(ktz);
	i2c_set_clientdata(client, ktz);
	dev_info(dev, "ktz8864 probe done (brightness via PM660L PWM)\n");
	return 0;
}

static int ktz8864_remove(struct i2c_client *client)
{
	struct ktz8864 *ktz = i2c_get_clientdata(client);
	if (ktz && ktz->en_gpio)
		gpiod_set_value_cansleep(ktz->en_gpio, 0);
	return 0;
}

static const struct of_device_id ktz8864_of_match[] = {
	{ .compatible = "ti,ktz8864" },
	{ }
};
MODULE_DEVICE_TABLE(of, ktz8864_of_match);

static const struct i2c_device_id ktz8864_id[] = {
	{ "ktz8864", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ktz8864_id);

static struct i2c_driver ktz8864_driver = {
	.driver = {
		.name = "ktz8864",
		.of_match_table = ktz8864_of_match,
	},
	.probe = ktz8864_probe,
	.remove = ktz8864_remove,
	.id_table = ktz8864_id,
};
module_i2c_driver(ktz8864_driver);
MODULE_DESCRIPTION("KTZ8864 backlight init driver (PWM dimming, I2C init only)");
MODULE_LICENSE("GPL v2");