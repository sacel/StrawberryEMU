/*
 * Copyright (C) 2010-2011 Strawberry-Pr0jcts <http://www.strawberry-pr0jcts.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DB2Stores.h"

#include "Logging/Log.h"
#include "SharedDefines.h"
#include "SpellMgr.h"

#include "DB2fmt.h"

#include <map>

DB2Storage <ItemEntry>                    sItemStore(Itemfmt);
typedef std::list<std::string> StoreProblemList1;
uint32 DB2FileCount = 0;

static bool LoadDB2_assert_print(uint32 fsize,uint32 rsize, const std::string& filename)
{
    sLog->outError("Size of '%s' setted by format string (%u) not equal size of C++ structure (%u).",filename.c_str(),fsize,rsize);

    // ASSERT must fail after function call
    return false;
}

struct LocalDB2Data
{
    LocalDB2Data(LocaleConstant loc)
        : defaultLocale(loc), availableDb2Locales(0xFFFFFFFF) {}

    LocaleConstant defaultLocale;

    // bitmasks for index of fullLocaleNameList
    uint32 availableDb2Locales;
};

template<class T>
inline void LoadDB2(LocalDB2Data& localeData, StoreProblemList1& errors, DB2Storage<T>& storage, std::string const& db2Path, std::string const& filename)
{
    // compatibility format and C++ structure sizes
    ASSERT(DB2FileLoader::GetFormatRecordSize(storage.GetFormat()) == sizeof(T) || LoadDB2_assert_print(DB2FileLoader::GetFormatRecordSize(storage.GetFormat()), sizeof(T), filename));

    ++DB2FileCount;
    std::string db2Filename = db2Path + filename;
    if (storage.Load(db2Filename.c_str(), localeData.defaultLocale))
    {
        for(uint8 i = 0; fullLocaleNameList[i].name; ++i)
        {
            if (!(localeData.availableDb2Locales & (1 << i)))
                continue;

            LocaleNameStr const* localStr = &fullLocaleNameList[i];

            std::string db2_dir_loc = db2Path + localStr->name + "/";

            std::string localizedName = db2Path + localStr->name + "/" + filename;
            if(!storage.LoadStringsFrom(localizedName.c_str(), localStr->locale))
                localeData.availableDb2Locales &= ~(1<<i);  // mark as not available for speedup next checks
        }
    }
    else
    {
        // sort problematic db2 to (1) non compatible and (2) nonexistent
        if (FILE* f = fopen(db2Filename.c_str(), "rb"))
        {
            char buf[100];
            snprintf(buf, 100, " (exist, but have %d fields instead " SIZEFMTD ") Wrong client version DB2 file?", storage.GetFieldCount(), strlen(storage.GetFormat()));
            errors.push_back(db2Filename + buf);
            fclose(f);
        }
        else
            errors.push_back(db2Filename);
    }
}

void LoadDB2Stores(const std::string& dataPath)
{
    uint32 oldMSTime = getMSTime();
    std::string db2Path = dataPath + "dbc/";

    LocaleNameStr const* defaultLocaleNameStr = NULL;

    StoreProblemList1 bad_db2_files;

    LocalDB2Data availableDb2Locales(LocaleConstant(0));//defaultLocaleNameStr->locale));

    LoadDB2(availableDb2Locales,bad_db2_files,sItemStore,                db2Path,"Item.db2");

    // error checks
    if (bad_db2_files.size() >= DB2FileCount)
    {
        sLog->outError("Incorrect DataDir value in worldserver.conf or ALL required *.db2 files (%d) not found by path: %sdb2", DB2FileCount, dataPath.c_str());
        exit(1);
    }
    else if (!bad_db2_files.empty())
    {
        std::string str;
        for (StoreProblemList1::iterator i = bad_db2_files.begin(); i != bad_db2_files.end(); ++i)
            str += *i + "\n";

        sLog->outError("Some required *.db2 files (%u from %d) not found or not compatible:\n%s",(uint32)bad_db2_files.size(), DB2FileCount, str.c_str());
        exit(1);
    }

    // Check loaded DBC files proper version
    if (!sItemStore.LookupEntry(68815))                     // last client known item added in 4.0.6a
    {
        sLog->outString("");
        sLog->outError("Please extract correct db2 files from build %s", AcceptableClientBuildsListStr().c_str());
        exit(1);
    }
    sLog->outString(">> Initialized %d DB2 data stores in %u ms", DB2FileCount, GetMSTimeDiffToNow(oldMSTime));
    sLog->outString();
}
