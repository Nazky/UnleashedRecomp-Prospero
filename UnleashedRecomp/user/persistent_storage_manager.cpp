#include "persistent_storage_manager.h"
#include <install/installer.h>
#include <os/logger.h>
#include <user/paths.h>
#if defined(__PROSPERO__)
#include <cstdio>
#include <sys/stat.h>
#endif

bool PersistentStorageManager::ShouldDisplayDLCMessage(bool setOffendingDLCFlag)
{
    if (BinStatus != EExtBinStatus::Success)
        return true;

    static std::unordered_map<EDLCFlag, DLC> flags =
    {
        { EDLCFlag::ApotosAndShamar, DLC::ApotosShamar },
        { EDLCFlag::Spagonia, DLC::Spagonia },
        { EDLCFlag::Chunnan, DLC::Chunnan },
        { EDLCFlag::Mazuri, DLC::Mazuri },
        { EDLCFlag::Holoska, DLC::Holoska },
        { EDLCFlag::EmpireCityAndAdabat, DLC::EmpireCityAdabat }
    };

    auto result = false;

    for (auto& pair : flags)
    {
        if (!Data.DLCFlags[(int)pair.first] && Installer::checkDLCInstall(GetGamePath(), pair.second))
        {
            if (setOffendingDLCFlag)
                Data.DLCFlags[(int)pair.first] = true;

            result = true;
        }
    }

    return result;
}

bool PersistentStorageManager::LoadBinary()
{
    BinStatus = EExtBinStatus::Success;

    auto dataPath = GetDataPath(true);

#if defined(__PROSPERO__)
    FILE* fp = fopen(dataPath.string().c_str(), "rb");
    if (!fp)
    {
        dataPath = GetDataPath(false);
        fp = fopen(dataPath.string().c_str(), "rb");
        if (!fp)
            return true;
    }

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize != (long)sizeof(PersistentData))
    {
        fclose(fp);
        BinStatus = EExtBinStatus::BadFileSize;
        return false;
    }

    PersistentData data{};
    if (fread(&data, 1, sizeof(data), fp) != sizeof(data))
    {
        fclose(fp);
        BinStatus = EExtBinStatus::IOError;
        return false;
    }
    fclose(fp);

    if (!data.VerifySignature())
    {
        BinStatus = EExtBinStatus::BadSignature;
        return false;
    }

    if (!data.VerifyVersion())
    {
        BinStatus = EExtBinStatus::BadVersion;
        return false;
    }

    memcpy(&Data, &data, sizeof(PersistentData));
    return true;
#else
    std::error_code ec;
    if (!std::filesystem::exists(dataPath, ec))
    {
        // Try loading base persistent data as fallback.
        dataPath = GetDataPath(false);

        if (!std::filesystem::exists(dataPath, ec))
            return true;
    }

    auto fileSize = std::filesystem::file_size(dataPath, ec);
    auto dataSize = sizeof(PersistentData);

    if (fileSize != dataSize)
    {
        BinStatus = EExtBinStatus::BadFileSize;
        return false;
    }

    std::ifstream file(dataPath, std::ios::binary);

    if (!file)
    {
        BinStatus = EExtBinStatus::IOError;
        return false;
    }

    PersistentData data{};

    file.read((char*)&data.Signature, sizeof(data.Signature));

    if (!data.VerifySignature())
    {
        BinStatus = EExtBinStatus::BadSignature;
        file.close();
        return false;
    }

    file.read((char*)&data.Version, sizeof(data.Version));

    if (!data.VerifyVersion())
    {
        BinStatus = EExtBinStatus::BadVersion;
        file.close();
        return false;
    }

    file.seekg(0);
    file.read((char*)&data, sizeof(data));
    file.close();

    memcpy(&Data, &data, dataSize);

    return true;
#endif
}

bool PersistentStorageManager::SaveBinary()
{
    LOGN("Saving persistent storage binary...");

    auto dataPath = GetDataPath(true);
#if defined(__PROSPERO__)
    mkdir(GetUserPath().string().c_str(), 0777);
    mkdir(dataPath.parent_path().string().c_str(), 0777);

    FILE* fp = fopen(dataPath.string().c_str(), "wb");
    if (!fp)
    {
        LOGN_ERROR("Failed to write persistent storage binary.");
        return false;
    }

    fwrite(&Data, 1, sizeof(PersistentData), fp);
    fflush(fp);
    fclose(fp);

    BinStatus = EExtBinStatus::Success;
    return true;
#else
    std::error_code ec;
    std::filesystem::create_directories(dataPath.parent_path(), ec);
    std::ofstream file(dataPath, std::ios::binary);

    if (!file)
    {
        LOGN_ERROR("Failed to write persistent storage binary.");
        return false;
    }

    file.write((const char*)&Data, sizeof(PersistentData));
    file.close();

    BinStatus = EExtBinStatus::Success;

    return true;
#endif
}
