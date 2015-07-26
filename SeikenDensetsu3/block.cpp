#include <map>
#include "block.hpp"
#include "huffman.hpp"

using namespace std;

vector<Block>* getBlocks(uint8_t* rom)
{
    map<uint32_t,uint16_t> blocksMap;
    for (int i = 0; i < 0x1000; i++)
    {
        uint8_t    bank = rom[0xEA91 + (rom[0x3E4E00 + i/2] & 0xF)];
        uint16_t offset = ((uint16_t*) &rom[0x3E2E00])[i];
        uint32_t   addr = ((bank << 16) + offset) - 0xC00000;

        blocksMap[addr] = i;
    }
    blocksMap.erase(0x390055);
    blocksMap.erase(0x394C2D);

    auto blocks = new vector<Block>;
    for (auto& block: blocksMap)
    {
        uint16_t    i = block.second;
        uint32_t addr = block.first;
        bool   isText = addr < 0x3BD079 and !(addr >= 0x3B0000 and addr < 0x3B4C2E);

        blocks->emplace_back(rom, i, addr, isText);
    }

    for (int i = 0; i < blocks->size() - 1; i++)
        blocks->at(i).init(blocks->at(i+1).addr);
    blocks->back().init(0x3C0000);

    return blocks;
}

void Block::init(uint32_t nextAddr)
{
    this->comprData = (uint16_t*)(rom + addr);
    this->comprSize = nextAddr  - this->addr;
}

void Block::decompress()
{
    data = Huffman::decompress(comprData, comprSize, (uint16_t*)(rom + 0x3E6600));
}

bool Block::check(vector<uint8_t>::iterator begin, vector<uint8_t>::iterator end)
{
    if (distance(begin, end) < 7)
        return false;

    if (*begin != 0x7B and not ((*(begin+1) >= 0x10 and *(begin+1) <= 0x1F) or
                                (*(begin+1) >= 0xC0 and *(begin+1) <= 0xCF)))
        return false;

    return true;
}

void Block::extract()
{
    if (data == nullptr)
        this->decompress();
    if (!isText)
        return;

    auto it = data->begin();
    while (it < data->end())
    {
        if (*it == 0x58 or *it == 0x5E or *it == 0x7B)
        {
            auto begin = it;

            int n = (*it == 0x7B) ? 6 : 1;
            while (n > 0)
            {
                while (it+1 < data->end() and !(*it == 0xFF and *(it+1) == 0xFF))
                    it++;
                if (it+1 >= data->end())
                    return;
                it += 2;
                n--;
            }

            if (n > 0)
                return;

            if (check(begin, it))
                sentences.emplace_back(distance(data->begin(), begin), begin, it);
            else
                it = begin + 1;
        }
        else
            it++;
    }
}
