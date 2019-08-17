#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <memory>
#include <stdexcept>
#include <limits>

constexpr const char *PROGRAM_NAME = "NFSU2SaveE";
constexpr int MAJOR_VERSION = 1;
constexpr int MINOR_VERSION = 0;

std::ostream& VERSION(std::ostream& cout)
{
	cout << MAJOR_VERSION << "." << MINOR_VERSION;
	return cout;
}

std::ostream& HELP(std::ostream& cout)
{
	cout << PROGRAM_NAME << " " << VERSION << " SaveFile [-b]";
	return cout;
}

bool file_exists(const char* filename)
{
	return std::ifstream(filename).good();
}

void backup(const char* in_filename, const char* backup_filename)
{
	std::ifstream in(in_filename, std::ios_base::binary);
	std::ofstream out(backup_filename, std::ios_base::binary);

	in.seekg(0, std::ios_base::end);
	std::streampos size = in.tellg();
	in.seekg(0);

	auto buffer = std::make_unique<char[]>(size);

	in.read(&buffer[0], size);
	out.write(&buffer[0], size);

	out.close();
	in.close();
}

class NFSU2SaveFile
{
	std::string filename;
	std::streampos size;
	std::unique_ptr<char[]> buffer;

public:
	NFSU2SaveFile() = delete;
	NFSU2SaveFile(std::string filename)
		: filename(filename)
	{
		/*
			"20CM"
		*/
		constexpr long HEADER_MAGIC_ID = 0x4D433032;

		std::ifstream stream(filename, std::ios_base::binary);
		if (!stream.good())
		{
			throw std::runtime_error("File \"" + filename + "\" cannot be read");
		}

		stream.seekg(0, std::ios_base::end);
		size = stream.tellg();
		stream.seekg(0);

		buffer = std::make_unique<char[]>(size);
		stream.read(&buffer[0], size);
		stream.close();

		/*
			"20CM" at 0th index and filesize at 4th index
		*/
		if (memcmp(&buffer[0], &HEADER_MAGIC_ID, 4) != 0 || memcmp(&buffer[4], &size, 2) != 0)
		{
			throw std::runtime_error("File \"" + filename + "\" not a valid save file");
		}
	}

	~NFSU2SaveFile()
	{
		std::ofstream out(filename, std::ios_base::binary);
		if (out.good())
		{
			out.write(&buffer[0], size);
			out.close();

			std::cout << "Changes saved...\n";
		}
	}

	const std::string get_profile_username() const
	{
		/*
			Profile Name is found at 0xD225
		*/
		return std::string(reinterpret_cast<char*>(&buffer[0xD225]));
	}

	const int32_t get_money() const noexcept
	{
		/*
			Money is stored as signed int32 at 0xA16A
		*/
		return *reinterpret_cast<int32_t*>(&buffer[0xA16A]);
	}

	void set_money(const int32_t new_money) noexcept
	{
		*reinterpret_cast<int32_t*>(&buffer[0xA16A]) = new_money;
	}

	const long car_slots_used() const noexcept
	{
		long result = 0;
		/*
			Index at which car slots information is stored
		*/
		constexpr long START_INDEX = 0x5AEC;

		/*
			Bytes used for each car slot information
		*/
		constexpr long SIZE_CAR_SLOT = 0x7F2;

		/*
			Maximum five cars can be stored in NFS U2
		*/
		constexpr long NUM_CAR_SLOTS = 5L;

		/*
			For memcmp
		*/
		constexpr long ZERO = 0L;

		for (long i = 0; i < NUM_CAR_SLOTS; i++)
		{
			if (memcmp(&buffer[START_INDEX + (i * SIZE_CAR_SLOT)], &ZERO, 2) != 0)
			{
				result++;
			}
		}

		return result;
	}

	enum class CAR_PERFORMACE
	{
		NILL_OUT, MAX_OUT
	};

	void change_car_performance(const int index, const CAR_PERFORMACE car_performance) noexcept
	{
		constexpr long START_INDEX = 0x5AEC;
		constexpr long SIZE_CAR_SLOT = 0x7F2;
		constexpr long NUM_CAR_SLOTS = 5;

		if (index < 0 || index >= NUM_CAR_SLOTS)
		{
			return;
		}

		/*
			Performance part starts after 0x94 of car slot information
			and there are 0x44 parts information, not packages but indiviual parts
		*/
		const long PATCH_POINT = START_INDEX + (index * SIZE_CAR_SLOT) + 0x94;
		constexpr long TOTAL_PARTS = 0x44;

		for (long i = 0; i < TOTAL_PARTS; i++)
		{
			buffer[PATCH_POINT + i] = (char)car_performance;
		}
	}

	friend std::ostream& operator <<(std::ostream& o, const NFSU2SaveFile& save)
	{
		o << "Profile Name: " << save.get_profile_username() << "\n";
		o << "Money: " << save.get_money() << "\n";
		o << "Car Slots Used: " << save.car_slots_used();

		return o;
	}
};

int main(int argc, char *argv[])
{
	if (argc == 1 || argc > 3)
	{
		std::cout << HELP << "\n";
		return 1;
	}

	if (!file_exists(argv[1]))
	{
		std::cout << "File \"" << argv[1] << "\" cannot be opened for reading\n";
		return 2;
	}

	bool do_backup = (argc == 3) && (strncmp(argv[2], "-b", 2) == 0);
	if (do_backup)
	{
		backup(argv[1], (std::string(argv[1]) + ".bak").c_str());
	}

	try
	{
		NFSU2SaveFile save_file(argv[1]);
		std::cout << save_file << "\n";

		/*
			Money is represented as a 32bit signed number
		*/
		int32_t new_money = 0;
		while (true)
		{
			std::string s_new_money;
			std::cout << "New Money(-1 to not change): ";
			std::getline(std::cin, s_new_money);
			
			try
			{
				new_money = std::stol(s_new_money);
				break;
			}
			catch (const std::invalid_argument& e)
			{
				continue;
			}
			catch (const std::out_of_range& e)
			{
				continue;
			}
		}
		
		
		std::cout << "\n";

		if (new_money != -1 && new_money > 0)
		{
			save_file.set_money(new_money);
		}

		const long CAR_SLOTS_USED = save_file.car_slots_used();
		for (long i = 0; i < CAR_SLOTS_USED; i++)
		{
			int32_t option = 0;
			while (true)
			{
				std::string s_option;
				std::cout << "Change performance of car " << i + 1 << "? (0 Nill, 1 Max, 2 No effect): ";
				std::getline(std::cin, s_option);

				try
				{
					option = std::stol(s_option);
					break;
				}
				catch (const std::invalid_argument& e)
				{
					continue;
				}
				catch (const std::out_of_range& e)
				{
					continue;
				}
			}
			

			if (option == 0 || option == 1)
			{
				if (option == 0)
					save_file.change_car_performance(i, NFSU2SaveFile::CAR_PERFORMACE::NILL_OUT);

				if (option == 1)
					save_file.change_car_performance(i, NFSU2SaveFile::CAR_PERFORMACE::MAX_OUT);
			}
		}
	}
	catch (const std::runtime_error& e)
	{
		std::cout << e.what() << "\n";
		return 2;
	}
}