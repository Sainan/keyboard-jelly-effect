#include <thread>

#include <AnalogueKeyboard.hpp>
#include <console.hpp>
#include <kbRgb.hpp>
#include <Thread.hpp>
#include <Vector2.hpp>

using namespace soup;

static bool running = true;
static UniquePtr<kbRgb> kr;
static Rgb colours[6 * 21];

int main()
{
	atexit([]
	{
		running = false;
		if (kr)
		{
			kr->deinit();
		}
	});
	console.overrideCtrlC([]
	{
		running = false;
		if (kr)
		{
			kr->deinit();
		}
		exit(0);
	});
	for (auto& _kr : kbRgb::getAll())
	{
		kr = std::move(_kr);
		kr->init();
		Thread t([]
		{
			for (auto& ka : AnalogueKeyboard::getAll())
			{
				while (running)
				{
					// Map intensity
					uint8_t intensity[6 * 21]{ 0 };
					for (auto& key : ka.getActiveKeys())
					{
						auto [row, column] = kr->mapKeyToPos(key.sk);
						if (row != 0xff && column != 0xff)
						{
							Vector2 impact_point(row, column);
							for (uint8_t row = 0; row != kr->getNumRows(); ++row)
							{
								for (uint8_t column = 0; column != kr->getNumColumns(); ++column)
								{
									const float dist = impact_point.distance(Vector2(row, column));
									if (dist <= 10.0f)
									{
										const Percentage<uint8_t> closeness = (1.0f - (dist / 10.0f));
#if true // additive
										auto key_intensity = static_cast<unsigned int>(key.value * closeness);
										key_intensity += intensity[row * kr->getNumColumns() + column];
										intensity[row * kr->getNumColumns() + column] = static_cast<uint8_t>(std::min(255u, key_intensity));
#else
										auto key_intensity = static_cast<uint8_t>(key.value * closeness);
										if (key_intensity > intensity[row * kr->getNumColumns() + column])
										{
											intensity[row * kr->getNumColumns() + column] = key_intensity;
										}
#endif
									}
								}
							}
						}
					}

					// Map intensity to colours
					for (size_t i = 0; i != 6 * 21; ++i)
					{
						colours[i] = Rgb(0, intensity[i], 0);
					}
				}
			}
		});
		while (running)
		{
			// Map colours to keys
			Rgb keys[NUM_KEYS];
			kr->mapPosToKeys(keys, colours, 6, 21);
			kr->setKeys(keys);

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}
