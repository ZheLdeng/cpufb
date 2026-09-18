#ifndef _TABLE_HPP
#define _TABLE_HPP

#include <ostream>
#include <string>
#include <vector>

class Table
{
public:
    Table();
    ~Table();

    Table(const Table &) = delete;
    Table &operator=(const Table &) = delete;

    void setColumnNum(int col);
    void addOneItem(std::vector<std::string> &item);
    void print();
    void writeCompact(std::ostream &out,
        char delimiter,
        const std::string &rowPrefix = "") const;
    int getCol();

private:
    int col;
    std::vector<int> colWidths;
    std::vector<std::vector<std::string> > contents;
};

#endif

