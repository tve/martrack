// Copyright (c) 2018 by Thorsten von Eicken
//

#if 0
typedef struct LogEntry {
    // 0
    int32_t time;                   // seconds since 2000-01-01
    // 4
    uint8_t version :5;             // firmware version (0.1-3.1)
    uint8_t type :3;                // reading type, see union below
    uint8_t flags;                  // status flags
    uint16_t events;                // events since previous entry
    // 8
    union {
        uint32_t _ [6];
        struct {                    // type 1: gps info
            uint32_t lat, lon;      // minutes*1E3
            uint32_t alt;           // m*10
            uint16_t speed;         // cm/s
            uint16_t course;        // degrees*1E2
            uint16_t hdop;          // *1E2
            uint8_t  sats;          // satellites used for fix
            // 19
            uint8_t  spare[9];
            // 28
            uint16_t iTemp;         // internal temperature Cx10
            uint16_t vCell;         // LiPo voltage mV
            // 32
        } gps;
        struct {                    // type 2: message
            char text [24];         // message text
            // 32
        } message;
    };
    // 32

    LogEntry () { memset(this, 0, sizeof *this); }
} LogEntry;
#endif

template< typename SF >
struct Logger {
    // FIFO list of LogEntry's is kept by `first` and `next`. Flash chip always has the current
    // sector erased so we can write into it as well as the next sector erased so we can "wrap"
    // over into it when we reach the end of the current sector.

    int off;        // offset into eeprom to store first & next;
    int total;      // total number of slots in flash
    int first;      // slot index of first entry in list
    int next;       // slot index of next entry to write (one past last)

    static constexpr int LEsize = sizeof(LogEntry);
    static constexpr int pageBits = 8; // 256 byte pages
    static constexpr int sectorBits = 12; // 4Kbyte sectors

    // init the logger and return true if all OK, the eepromOffset determines where the logger
    // state is saved (uses 12 bytes).
    bool init(int eepromOffset) {
        total = (SF::size()*1024) / LEsize;
        //total = (6*4*1024) / LEsize; // hack for testing, makes flash real small
        off = eepromOffset;
        first = EEPROM::read32(off+0);
        next = EEPROM::read32(off+4);
        int chk = EEPROM::read32(off+8);

        if (chk != (first ^ next ^ (int)0xbeeff00d)) {
            first = 0;
            next = 0;
            save();
            SF::erase(0);
            SF::erase(1<<sectorBits);
        }
        return true;
    }

    // eraseAll fully wipes the flash chip and resets the logger
    void eraseAll() {
        first = 0;
        next = 0;
        save();
        SF::wipe();
    }

    // save is an internal function to save the state to eeprom
    void save() {
        EEPROM::write32(off+0, first);
        EEPROM::write32(off+4, next);
        EEPROM::write32(off+8, first ^ next ^ 0xbeeff00d);
    }

    // count returns the number of entries logged
    int count() { return next>=first ? next-first : next-first+total; }
    // size returns the total number of entries
    int size() {
        constexpr int freeSect = ((1<<sectorBits)+LEsize-1) / LEsize; // 1 sect always erased
        return total - freeSect;
    }

    // pushEntry adds an entry to the end of the list
    void pushEntry(LogEntry &le) {
        // slot index of next after the write ("next-next")
        int nn = next+1;
        if (nn >= total) nn = 0;

        // figure out where we're writing
        int addr = next*LEsize;
        int page1 = addr >> pageBits;
        int page2 = (addr+LEsize-1) >> pageBits;
        if (page1 == page2) {
            // all fits in same page
            //printf("write(%d/%d, %d)\r\n", addr, addr>>pageBits, LEsize);
            SF::write(addr, &le, LEsize);
        } else {
            // write straddles two pages
            int sz1 = (page2<<8)-addr;
            SF::write(addr, &le, sz1);
            SF::write(page2<<8, ((uint8_t*)&le)+sz1, LEsize-sz1);
            //printf("write(%d/%d, %d)(%d/%d, %d)\r\n",
            //        addr, addr>>pageBits, sz1, page2<<pageBits, page2, LEsize-sz1);
        }

        bool x = (nn*LEsize)>>sectorBits != addr>>sectorBits; // crossing sectors?
        next = nn;
        if (x) {
            // erase sector after next
            int sect = ((nn*LEsize)>>sectorBits) + 1;
            if (sect<<sectorBits >= SF::size()<<10) sect = 0;
            if (sect<<sectorBits >= 6*4*1024) sect = 0;
            //printf("*** erase(%d/%d)\r\n", sect<<sectorBits, sect<<4);
            SF::erase(sect<<sectorBits);

            // see whether we erased the head of the fifo...
            if (((first*LEsize)>>sectorBits) == sect) {
                while (((first*LEsize)>>sectorBits) == sect) first++; // TODO: optimize
                if (first >= total) first = 0;
                printf("flash full, first now %d\r\n", first);
            }
            save();
        }
    }

    // firstEntry returns the entry at head of list without removing it. Returns false if the list
    // is empty.
    bool firstEntry(LogEntry *le) {
        if (first == next) return false;
        //printf("read(%d, %d)\r\n", first*LEsize,LEsize);
        SF::read(first*LEsize, le, LEsize);
        return true;
    }

    // shiftEntry removes the entry at the head of the list.
    void shiftEntry() {
        int ff = first+1;
        if (ff >= total) ff = 0;

        bool x = first>>8 != ff>>8; // crossing page boundary, save
        first = ff;
        if (x) save();
    }

#if 0
    void firstEntry(LogEntry *le); // return entry at head of list, don't remove, false=no-entry
    void lastEntry(LogEntry *le); // return entry at tail of list, don't remove, false=no-entry
    void popEntry(); // remove entry at tail of list
#endif

};
