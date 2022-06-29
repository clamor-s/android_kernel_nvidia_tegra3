/*
 * tegra_rt5631.c - Tegra machine ASoC driver for boards using RT5631 codec.
 *
 */

#include <asm/mach-types.h>

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"
#include <mach/tegra_asoc_pdata.h>

#define DRV_NAME "tegra-snd-codec"

struct tegra_rt5631 {
	struct tegra_asoc_utils_data util_data;
};

static struct snd_soc_ops tegra_rt5631_ops;
static struct snd_soc_ops tegra_spdif_ops;

static struct snd_soc_dai_link tegra_rt5631_dai[] = {
	{
		.name = "RT5631",
		.stream_name = "RT5631 PCM",
		.codec_name = "rt5631.4-001a",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra30-i2s.1",
		.codec_dai_name = "rt5631-hifi",
		.ops = &tegra_rt5631_ops,
	},
	{
		.name = "SPDIF",
		.stream_name = "SPDIF PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra30-spdif",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_spdif_ops,
	}
};

static struct snd_soc_card snd_soc_tegra_rt5631 = {
	.name = "tegra-codec",
	.dai_link = tegra_rt5631_dai,
	.num_links = ARRAY_SIZE(tegra_rt5631_dai),
};

static __devinit int tegra_rt5631_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_rt5631;
	struct tegra_rt5631 *machine;
	struct tegra_asoc_platform_data *pdata;

	int ret;
	printk("%s+\n", __func__);

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	machine = kzalloc(sizeof(struct tegra_rt5631), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_rt5631 struct\n");
		return -ENOMEM;
	}

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	printk("%s-\n", __func__);
	return 0;

err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit tegra_rt5631_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_rt5631 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&machine->util_data);

	kfree(machine);

	return 0;
}

static struct platform_driver tegra_rt5631_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_rt5631_driver_probe,
	.remove = __devexit_p(tegra_rt5631_driver_remove),
};

static int __init tegra_rt5631_modinit(void)
{
	return platform_driver_register(&tegra_rt5631_driver);
}
module_init(tegra_rt5631_modinit);

static void __exit tegra_rt5631_modexit(void)
{
	platform_driver_unregister(&tegra_rt5631_driver);
}
module_exit(tegra_rt5631_modexit);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra+RT5631 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
