#include "table.hpp"

#include <iostream>
using namespace std;

static void write_compact_cell(ostream &out, const string &cell, char delimiter)
{
    if (delimiter != ',') {
        out << cell;
        return;
    }

    bool need_quote = cell.find_first_of(",\"\n\r") != string::npos;
    if (!need_quote) {
        out << cell;
        return;
    }

    out << '"';
    for (char c : cell) {
        if (c == '"') out << '"';
        out << c;
    }
    out << '"';
}

Table::Table()
{
    col = 0;
}

Table::~Table() {}

void Table::setColumnNum(int col)
{
    int i;

    this->col = col;

    colWidths.resize(col);

    for (i = 0; i < col; i++) {
        colWidths[i] = 2;
    }
}

void Table::addOneItem(std::vector<std::string> &item)
{
    int i;

    contents.push_back(item);

    for (i = 0; i < col; i++) {
        int length = item[i].size() + 2;
        if (length > colWidths[i]) {
            colWidths[i] = length;
        }
    }
}

void Table::print()
{
    int i, j, k;

    int tableWidth = col + 1;
    for (i = 0; i < col; i++) {
        tableWidth += colWidths[i];
    }

    string vLine(tableWidth, '-');
    cout << vLine << endl;

    for (i = 0; i < contents.size(); i++) {
        string oneLine("|");
        for (j = 0; j < col; j++) {
            oneLine += (" " + contents[i][j]);
            for (k = 1 + contents[i][j].size(); k < colWidths[j]; k++) {
                oneLine += " ";
            }
            oneLine += "|";
        }
        cout << oneLine << endl;
    }

    cout << vLine << endl;
}

void Table::writeCompact(
    ostream &out, char delimiter, const string &rowPrefix) const
{
    for (size_t i = 0; i < contents.size(); i++) {
        if (!rowPrefix.empty()) {
            write_compact_cell(out, i == 0 ? "section" : rowPrefix, delimiter);
            if (col > 0) out << delimiter;
        }

        for (int j = 0; j < col; j++) {
            if (j != 0) out << delimiter;
            write_compact_cell(out, contents[i][j], delimiter);
        }
        out << '\n';
    }
}

int Table::getCol()
{
    return col;
}
