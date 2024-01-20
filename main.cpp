#include <thread>

#include <AnalogueKeyboard.hpp>
#include <console.hpp>
#include <kbRgb.hpp>
#include <Keyboard.hpp>
#include <Thread.hpp>
#include <Vector2.hpp>

using namespace soup;

static bool running = true;
static AnalogueKeyboard* ka; // Using a pointer because compiler is too stupid to move it
static UniquePtr<kbRgb> kr;
static Rgb colours[kbRgb::MAX_ROWS * kbRgb::MAX_COLUMNS];

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
	for (auto& kbd : Keyboard::getAll())
	{
		if (!kbd.analogue || !kbd.rgb)
		{
			continue;
		}
		std::cout << "Input: " << kbd.analogue->name << "\n";
		std::cout << "Output: " << kbd.rgb->name << "\n";
		ka = &*kbd.analogue;
		kr = std::move(kbd.rgb);
		Thread t([](Capture&&)
		{
			while (running)
			{
				// Map intensity
				uint8_t intensity[kbRgb::MAX_ROWS * kbRgb::MAX_COLUMNS]{ 0 };
				for (auto& key : ka->getActiveKeys())
				{
					auto [row, column] = kr->mapKeyToPos(key.sk).at(0);
					if (row != 0xff && column != 0xff)
					{
						Vector2 impact_point(row, column);
						for (uint8_t row = 0; row != kr->getNumRows(); ++row)
						{
							for (uint8_t column = 0; column != kbRgb::MAX_COLUMNS; ++column)
							{
								const float dist = impact_point.distance(Vector2(row, column));
								if (dist <= 10.0f)
								{
									const Percentage<uint8_t> closeness = (1.0f - (dist / 10.0f));
#if true // additive
									auto key_intensity = static_cast<unsigned int>(key.value * closeness);
									key_intensity += intensity[row * kbRgb::MAX_COLUMNS + column];
									intensity[row * kbRgb::MAX_COLUMNS + column] = static_cast<uint8_t>(std::min(255u, key_intensity));
#else
									auto key_intensity = static_cast<uint8_t>(key.value * closeness);
									if (key_intensity > intensity[row * kbRgb::MAX_COLUMNS + column])
									{
										intensity[row * kbRgb::MAX_COLUMNS + column] = key_intensity;
									}
#endif
								}
							}
						}
					}
				}

				// Map intensity to colours
				for (size_t i = 0; i != kbRgb::MAX_ROWS * kbRgb::MAX_COLUMNS; ++i)
				{
					colours[i] = Rgb(0, intensity[i], 0);
				}

				if (ka->disconnected)
				{
					std::cout << "Keyboard disconnected\n";
					running = false;
				}
			}
		});
		while (running)
		{
			// Map colours to keys
			Rgb keys[NUM_KEYS];
			kr->mapPosToKeys(keys, colours, kbRgb::MAX_ROWS, kbRgb::MAX_COLUMNS);
			kr->setKeys(keys);

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		break;
	}
	if (running)
	{
		std::cout << "No compatible (analogue + RGB) keyboard found.\n";
		return 1;
	}
	return 0;
}
