#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <unordered_map>

using namespace std;
namespace fs = std::filesystem;

vector<char> symbolData;


// data used to read CSV files
vector<string> filesToRead;
string fileAddress = "";
string fileLine = "";
int fileLineIndex = 0;
int fileLineLength = 0;
int fileLineSpot = 0;
int fileLineSpot1 = 0;

// prices and date indices that have not been filled (inside p[] and d[])
#define DUMMY_INT INT_MIN
// empty entries[] and exits[] values (unnecessary)
#define DUMMY_DOUBLE (double)INT_MIN

#define maxNumSymbols 25000
#define maxNumDates 1300
#define numStoredTimes 3
// times whose bars should be stored and used to backtest
vector<int> storedTimes = { 930, 935, 1500 };
// metrics (OHLCV) of the bars that should be stored and used to backtest
vector<int> storedMetrics = { 'O', 'C', 'C' };

string columnCodes = "";


int numFiles = 0;

long long numBarsTotal = 0;
int numBars[maxNumSymbols];


// file parsing info
int currentSymbolIndex = 0;
string currentSymbol = "";
long long currentDateTime = 0;
int currentDate = 0;
int currentDateIndex = 0;
int currentTime = 0;
int currentTimeIndex = 0;
vector<int> currentOHLCV = {0,0,0,0,0};


// data points
int p[maxNumSymbols * maxNumDates * numStoredTimes];


// date to index and back mapping
int numDates = 0;
unordered_map<int, int> date_index;
unordered_map<int, int> index_date;

// symbol to index and back mapping (all symbols in dataset)
int numSymbols = 0;
unordered_map<string, int> symbol_index;
unordered_map<int, string> index_symbol;

// number of symbols found in each date
int numBarsByDateIndex[maxNumDates];
// values for each day and symbol used to determine trade entry and exit values
unordered_map<string, int> symbol_index_day[maxNumDates];
unordered_map<int, string> index_symbol_day[maxNumDates];

// number of symbols with all storedTimes values in p[] filled
int numFilledByDateIndex[maxNumDates];
// values for each day and symbol (only ones with all times within that day and symbol filled) used to determine trade entry and exit values
unordered_map<string, int> symbol_index_sorted[maxNumDates];
unordered_map<int, string> index_symbol_sorted[maxNumDates];


double** entries;
double** exits;

// values that sweep across all dates, filling in holes in price with last price value recorded for that symbol
int priceSweeper[maxNumSymbols];
long long lastDateTime[maxNumSymbols];

// cumulative volume for each symbol and date
long long totalVolume[maxNumSymbols][maxNumDates];


// predefined user-created symbol information for user symbol organization and volume/market cap filtering during backtesting
string userTickers[maxNumSymbols];
long long userVolumes[maxNumSymbols];
long long userMarketCaps[maxNumSymbols];

// symbols allowed to trade. filter by volume and market cap using data from the above arrays.
int numAllowedTickers = 0;
unordered_map<string, int> allowed_index;
unordered_map<int, string> index_allowed;


long long power(int b, int e) {
    if (e < 0) return 0;
    if (e == 0) return 1;
    long long n = b;
    for (int i = 1; i < e; i++) {
        n *= (long long)b;
    }
    return n;
}

bool isNumeric(char c) {
    return (c >= '0' && c <= '9');
}

string dateToString(int d) {
    if(d < 20000000 || d > 20291231){
        cerr << "Date " << d << " is out of the range 20000000 to 20291231.\n\n";
        exit(1);
    }
    return to_string(d);
}

// parse a ticker symbol within a CSV file line
string parseSymbol(){
    string ticker = "";
    fileLineSpot1 = fileLineSpot;
    while (fileLine[fileLineSpot1] != ',') {
        fileLineSpot1++;
        if(fileLineSpot1 >= fileLineLength){
            break;
        }
    }
    if(fileLineSpot1 - fileLineSpot < 1 || fileLineSpot1 - fileLineSpot > 10){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a ticker symbol of length " << fileLineSpot1 - fileLineSpot << ", but the length must be from 1 through 10.\n\n";
        exit(1);
    }
    return fileLine.substr(fileLineSpot, fileLineSpot1 - fileLineSpot);
}

// parse a time within a CSV file line
int parseTime(){
    
    if(fileLineSpot + 4 >= fileLineLength){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended before full 5-character time value was read.\n\n";
        exit(1);
    }

    if (!(isNumeric(fileLine[fileLineSpot]) && isNumeric(fileLine[fileLineSpot + 1]) && isNumeric(fileLine[fileLineSpot + 3]) && isNumeric(fileLine[fileLineSpot + 4]) && fileLine[fileLineSpot + 2] == ':')) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct time format (HH:MM) for the moment value.\n\n";
        exit(1);
    }

    // time
    int time = (fileLine[fileLineSpot] - '0') * 1000 + (fileLine[fileLineSpot + 1] - '0') * 100 + (fileLine[fileLineSpot + 2] - '0') * 10 + (fileLine[fileLineSpot + 3] - '0');
    if (time < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time before 00:00.\n\n";
        exit(1);
    }
    if (time >= 2400) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time after 23:59.\n\n";
        exit(1);
    }
    
    return time;
}

// parse a date within a CSV file line
int parseDate(){
    
    if(fileLineSpot + 9 >= fileLineLength){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended before full 10-character date value was read.\n\n";
        exit(1);
    }

    if (!(isNumeric(fileLine[fileLineSpot]) && isNumeric(fileLine[fileLineSpot + 1]) && isNumeric(fileLine[fileLineSpot + 2]) && isNumeric(fileLine[fileLineSpot + 3]) && isNumeric(fileLine[fileLineSpot + 5]) && isNumeric(fileLine[fileLineSpot + 6]) && isNumeric(fileLine[fileLineSpot + 8]) && isNumeric(fileLine[fileLineSpot + 9]) && fileLine[fileLineSpot + 4] == '-' && fileLine[fileLineSpot + 7] == '-')) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct date format (YYYY-MM-DD) for the moment value.\n\n";
        exit(1);
    }

    // date
    int date = (fileLine[fileLineSpot + 2] - '0') * 100000 + (fileLine[fileLineSpot + 3] - '0') * 10000 + (fileLine[fileLineSpot + 5] - '0') * 1000 + (fileLine[fileLineSpot + 6] - '0') * 100 + (fileLine[fileLineSpot + 8] - '0') * 10 + (fileLine[fileLineSpot + 9] - '0');
    if (date < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date before 2000/01/01.\n\n";
        exit(1);
    }
    if (date >= 11160) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date after 2030/01/01.\n\n";
        exit(1);
    }
    
    return date;
}

// parse a combined date and time within a CSV file line
long long parseMoment(){
    
    if(fileLineSpot + 15 >= fileLineLength){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended before full 16-character moment value was read.\n\n";
        exit(1);
    }

    if (!(isNumeric(fileLine[fileLineSpot]) && isNumeric(fileLine[fileLineSpot + 1]) && isNumeric(fileLine[fileLineSpot + 2]) && isNumeric(fileLine[fileLineSpot + 3]) && isNumeric(fileLine[fileLineSpot + 5]) && isNumeric(fileLine[fileLineSpot + 6]) && isNumeric(fileLine[fileLineSpot + 8]) && isNumeric(fileLine[fileLineSpot + 9]) && isNumeric(fileLine[fileLineSpot + 11]) && isNumeric(fileLine[fileLineSpot + 12]) && isNumeric(fileLine[fileLineSpot + 14]) && isNumeric(fileLine[fileLineSpot + 15]) && fileLine[fileLineSpot + 4] == '-' && fileLine[fileLineSpot + 7] == '-')) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct date and time format (YYYY-MM-DD HH:MM:SS or YYYY-MM-DD HH:MM) for the moment value.\n\n";
        exit(1);
    }
    

    // date
    int date = (fileLine[fileLineSpot + 2] - '0') * 100000 + (fileLine[fileLineSpot + 3] - '0') * 10000 + (fileLine[fileLineSpot + 5] - '0') * 1000 + (fileLine[fileLineSpot + 6] - '0') * 100 + (fileLine[fileLineSpot + 8] - '0') * 10 + (fileLine[fileLineSpot + 9] - '0');
    if (date < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date before 2000/01/01.\n\n";
        exit(1);
    }
    if (date >= 11160) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a date after 2030/01/01.\n\n";
        exit(1);
    }


    // time
    int time = (fileLine[fileLineSpot + 11] - '0') * 1000 + (fileLine[fileLineSpot + 12] - '0') * 100 + (fileLine[fileLineSpot + 14] - '0') * 10 + (fileLine[fileLineSpot + 15] - '0');
    if (time < 0) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time before 00:00.\n\n";
        exit(1);
    }
    if (time >= 2400) {
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " has a time after 23:59.\n\n";
        exit(1);
    }

    return date * 10000 + time;
}

// parse a Unix timestamp within a CSV file line
long long parseUnix(){

    while(fileLine[fileLineSpot1] != ','){
        if (!(isNumeric(fileLine[fileLineSpot1]))) {
            cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct Unix time format. It must contain only digits.\n\n";
            exit(1);
        }

        fileLineSpot1++;
        if (fileLineSpot1 >= fileLineLength) {
            break;
        }
    }

    if(fileLineSpot1 - fileLineSpot < 18 || fileLineSpot1 - fileLineSpot > 19){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct Unix time format. The value must be 18 or 19 digits long.\n\n";
        exit(1);
    }

    long long unix = 0;

    for(int i=fileLineSpot;i<fileLineSpot1-9;i++){
        unix *= 10;
        unix += fileLine[i] - '0';
    }
    
    if(unix < 946702800 || unix >= 1893474000){
        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " does not have the correct Unix time format. The value (excluding the last 9 digits) must be between 946702800 (Jan 1, 2000, 12:00 AM EST) and 1893474000 (Jan 1, 2030, 12:00 AM EST). It is " << unix << ".\n\n";
        exit(1);
    }
    
    // convert to date and time (year, month, day are 0-indexed) example output: 202503160930
    vector<int> yearStarts = {946702800, 978325200, 1009861200, 1041397200, 1072933200, 1104555600, 1136091600, 1167627600, 1199163600, 1230786000, 1262322000,
    1293858000, 1325394000, 1357016400, 1388552400, 1420088400, 1451624400, 1483246800, 1514782800, 1546318800, 1577854800, 1609477200, 1641013200, 1672549200,
    1704085200, 1735707600, 1767243600, 1798779600, 1830315600, 1861938000, 1893474000};
    vector<int> monthDays = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

    long long dateTime = 20000000;
    int yearStart = 946702800;

    int year = 0;
    for(int i=1;i<=30;i++){
        if(yearStarts[i] > unix){
            year = i - 1;
            yearStart = yearStarts[year];
            dateTime = 20000000 + year * 10000;
            break;
        }
    }
    
    int dayOfYear = (unix - yearStart) / 86400;
    int month = 0;
    int dayOfMonth = 0;
    if(year % 4 == 0){
        if(dayOfYear == 59){
            month = 2;
            dateTime += 200;
            dayOfMonth = 29;
        }else{
            if(dayOfYear > 59){
                dayOfYear--;
                for(month = 1; month <= 12; month++){
                    if(monthDays[month] > dayOfYear){
                        dayOfMonth = dayOfYear - monthDays[month - 1] + 1;
                        dateTime += month * 100 + dayOfMonth;
                        break;
                    }
                }
            }
        }
    }else{
        for(month = 1; month <= 12; month++){
            if(monthDays[month] > dayOfYear){
                dayOfMonth = dayOfYear - monthDays[month - 1] + 1;
                dateTime += month * 100 + dayOfMonth;
                break;
            }
        }
    }

    int time = ((unix - 14400) / 60) % 1440;

    time = (time / 60) * 100 + (time % 60);

    dateTime = dateTime * 10000 + time;
    
    return dateTime;
}

// parse an OHLCV value within a CSV file line
int parseOHLCV(char ohlcvCode){
    
    while (fileLine[fileLineSpot1] != ',' && fileLine[fileLineSpot1] != '.') {
        fileLineSpot1++;
        if (fileLineSpot1 >= fileLineLength) {
            break;
        }
    }
    int num = 0;
    // get digits before .
    for (int j = fileLineSpot; j < fileLineSpot1; j++) {
        if (ohlcvCode == 'V') { // volume stores value
            num += (fileLine[j] - '0') * (int)power(10, fileLineSpot1 - j - 1);
        }
        else { // others store 100 * value
            num += (fileLine[j] - '0') * (int)power(10, fileLineSpot1 - j + 1);
        }
    }
    if (ohlcvCode == 'V') {
        return num;
    }
    
    // get two digits after .
    if (fileLine[fileLineSpot1] == '.') {
        fileLineSpot1++;
        if(fileLineSpot1 < fileLineLength){
            if (isNumeric(fileLine[fileLineSpot1])) {
                num += (fileLine[fileLineSpot1] - '0') * 10;
                fileLineSpot1++;
                if(fileLineSpot1 < fileLineLength){
                    if (isNumeric(fileLine[fileLineSpot1])) {
                        num += (fileLine[fileLineSpot1] - '0');
                    }
                }
            }
        }
    }

    return num;
}

void checkColumnCodes(){
    int numS = 0, numM = 0, numD = 0, numT = 0, numO = 0, numH = 0, numL = 0, numC = 0, numV = 0;
    int n = size(columnCodes);
    if(n < 3 || n > 20){
        cerr << "The length of the CSV column code must be from 3 to 20 characters long. It is " << n << "characters long.\n\n";
        exit(1);
    }
    for(int i=0;i<n;i++){
        switch(columnCodes[i]){
            case 'S':
                numS++;
                break;
            case 'M':
            case 'U':
                numM++;
                break;
            case 'D':
                numD++;
                break;
            case 'T':
                numT++;
                break;
            case 'O':
                numO++;
                break;
            case 'H':
                numH++;
                break;
            case 'L':
                numL++;
                break;
            case 'C':
                numC++;
                break;
            case 'V':
                numV++;
                break;
            case '-':
                break;
            cerr << "The CSV column code may only contain the characters \"MDTSOHLCV-\".\n\n";
            exit(1);
        }
    }

    if(numS != 1){
        cerr << "The CSV column code must contain exactly one symbol value (S).\n\n";
        exit(1);
    }
    if(numM > 1 || numD > 1 || numT > 1){
        cerr << "The CSV column code must contain no more than one moment, Unix time, time, or date value (M/U/T/D).\n\n";
        exit(1);
    }
    if((numM != 1 && (numD != 1 || numT != 1)) || (numM == 1 && (numD == 1 || numT == 1))){
        cerr << "The CSV column code must contain exactly one moment or Unix time value, or one time and one date value (M/U/T/D).\n\n";
        exit(1);
    }
    if(numO != 1){
        cerr << "The CSV column code must contain exactly one open value (O).\n\n";
        exit(1);
    }
    if(numH != 1){
        cerr << "The CSV column code must contain exactly one high value (H).\n\n";
        exit(1);
    }
    if(numL != 1){
        cerr << "The CSV column code must contain exactly one low value (L).\n\n";
        exit(1);
    }
    if(numC != 1){
        cerr << "The CSV column code must contain exactly one close value (C).\n\n";
        exit(1);
    }
    if(numV != 1){
        cerr << "The CSV column code must contain exactly one volume value (V).\n\n";
        exit(1);
    }
}

// make sure the stored times and metrics are formatted properly
void checkStoredTimes(){
    if(numStoredTimes != size(storedTimes) || numStoredTimes != size(storedMetrics)){
        cerr << "The number of stored times, size of the storedTimes vector, and size of the storedMetrics vector must all be equal.\n\n";
        exit(1);
    }

    for(int i=0;i<numStoredTimes;i++){
        if(storedTimes[i] < 0 || storedTimes[i] > 2359){
            cerr << "storedTimes[" << i << "] must be between 0 and 2359.\n\n";
            exit(1);
        }
        if(!(storedMetrics[i] == 'O' || storedMetrics[i] == 'H' || storedMetrics[i] == 'L' || storedMetrics[i] == 'C' || storedMetrics[i] == 'V')){
            cerr << "storedMetrics[" << i << "] must be O/H/L/C/V.\n\n";
            exit(1);
        }

        for(int j=0;j<numStoredTimes;j++){
            if(i != j && storedTimes[i] == storedTimes[j]){
                cerr << "Two stored times cannot hold the same time.\n\n";
                exit(1);
            }
        }
    }
}

// read one file
void readFile(){
    int numValuesPerLine = size(columnCodes);

    ifstream f(fileAddress);

    if (!f.is_open()) {
        cerr << "File " << fileAddress << " was found but could not be opened.\n\n";
        exit(1);
    }

    fileLine = "";

    getline(f, fileLine);

    fileLineIndex = -1;
    while (getline(f, fileLine)) {
        fileLineIndex++;
        fileLineSpot = 0;
        fileLineSpot1 = 0;
        fileLineLength = fileLine.length();

        if (fileLineLength < 20) {
            cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " with a length of " << fileLineLength << "characters was incorrectly shorter than 20 characters long. Make sure it includes a date and time, OHLCV values, and a ticker symbol.\n\n";
            exit(1);
        }

        // reset currentTimeIndex so that it can be used to break line or keep reading
        currentTimeIndex = 0;

        // get every value in the line, then afterwards organize the values
        for(int i=0;i<numValuesPerLine;i++){
            fileLineSpot1 = fileLineSpot;

            // get this value
            switch(columnCodes[i]){
                case 'S':{
                    currentSymbol = parseSymbol();
                    if (symbol_index.find(currentSymbol) != symbol_index.end()) {
                        currentSymbolIndex = symbol_index.at(currentSymbol);
                    }else{
                        currentSymbolIndex = numSymbols;
                        symbol_index[currentSymbol] = numSymbols;
                        index_symbol[numSymbols] = currentSymbol;
                        numSymbols++;
                    }
                    break;
                }
                case 'M':{
                    currentDateTime = parseMoment();
                    currentDate = currentDateTime / 10000;
                    currentTime = currentDateTime % 10000;
                    currentTimeIndex = -1;
                    if (date_index.find(currentDate) != date_index.end()) {
                        currentDateIndex = date_index.at(currentDate);
                    }else{
                        currentDateIndex = numDates;
                        date_index[currentDate] = numDates;
                        index_date[numDates] = currentDate;
                        numDates++;
                    }
                    for(int i=0;i<numStoredTimes;i++){
                        if(storedTimes[i] == currentTime){
                            currentTimeIndex = i;
                            break;
                        }
                    }
                    break;
                }
                case 'D':{
                    currentDate = parseDate();
                    currentDateTime = currentDate * 10000 + currentTime;
                    if (date_index.find(currentDate) != date_index.end()) {
                        currentDateIndex = date_index.at(currentDate);
                    }else{
                        currentDateIndex = numDates;
                        date_index[currentDate] = numDates;
                        index_date[numDates] = currentDate;
                        numDates++;
                    }
                    break;
                }
                case 'T':{
                    currentTime = parseTime();
                    currentDateTime = currentDate * 10000 + currentTime;
                    currentTimeIndex = -1;
                    for(int i=0;i<numStoredTimes;i++){
                        if(storedTimes[i] == currentTime){
                            currentTimeIndex = i;
                            break;
                        }
                    }
                    break;
                }
                case 'O':{
                    currentOHLCV[0] = parseOHLCV('O');
                    break;
                }
                case 'H':{
                    currentOHLCV[1] = parseOHLCV('H');
                    break;
                }
                case 'L':{
                    currentOHLCV[2] = parseOHLCV('L');
                    break;
                }
                case 'C':{
                    currentOHLCV[3] = parseOHLCV('C');
                    break;
                }
                case 'V':{
                    currentOHLCV[4] = parseOHLCV('V');
                    break;
                }
                case 'U':{
                    currentDateTime = parseUnix();
                    currentDate = currentDateTime / 10000;
                    currentTime = currentDateTime % 10000;
                    currentTimeIndex = -1;
                    if (date_index.find(currentDate) != date_index.end()) {
                        currentDateIndex = date_index.at(currentDate);
                    }else{
                        currentDateIndex = numDates;
                        date_index[currentDate] = numDates;
                        index_date[numDates] = currentDate;
                        numDates++;
                    }
                    for(int i=0;i<numStoredTimes;i++){
                        if(storedTimes[i] == currentTime){
                            currentTimeIndex = i;
                            break;
                        }
                    }
                    break;
                }
            }

            // move to next comma if there is a next comma
            if(i < numValuesPerLine - 1){
                while (fileLine[fileLineSpot] != ',') {
                    fileLineSpot++;
                    if (fileLineSpot >= fileLineLength) {
                        cerr << "Bar " << fileLineIndex << " in file " << fileAddress << " ended without a comma after value " << i << ". According to the columnCodes you entered, there should be " << numValuesPerLine << " values and " << numValuesPerLine - 1 << " commas.\n\n";
                        exit(1);
                    }
                }
            }
            // move past it
            fileLineSpot++;
        }

        // update the map of symbols on this day if a new symbol is found
        int currentSymbolIndexDay = 0;
        if (symbol_index_day[currentDateIndex].find(currentSymbol) != symbol_index_day[currentDateIndex].end()) {
            currentSymbolIndexDay = symbol_index.at(currentSymbol);
        }else{
            currentSymbolIndexDay = numBarsByDateIndex[currentDateIndex];
            symbol_index_day[currentDateIndex][currentSymbol] = currentSymbolIndexDay;
            index_symbol_day[currentDateIndex][currentSymbolIndexDay] = currentSymbol;
            numBarsByDateIndex[currentDateIndex]++;
        }

        // if this isn't the first bar recorded of this symbol EVER (if it is, ascend/descend does not matter as there is no previous datetime to compare to)
        if(numBars[currentSymbolIndex] > 0){

            // handle out of ascending order exception
            if (currentDateTime <= lastDateTime[currentSymbolIndex]) {
                cerr << "Consecutively parsed bars " << lastDateTime[currentSymbolIndex] << " and " << currentDateTime << " of symbol " << index_symbol.at(currentSymbolIndex) << " were not recorded in ascending order as previous bars of this symbol and/or other symbols were.\n\n";
                exit(1);
            }
        }

        // debugging
        //if(currentDateIndex == 0 && currentSymbolIndex < 10){
        //    cout << currentSymbolIndex << " " << currentOHLCV[0] << " " << currentOHLCV[1] << " " << currentOHLCV[2] << " " << currentOHLCV[3] << " " << currentOHLCV[4] << " ";
        //    cout << currentSymbol << " " << currentDateTime << " " << currentDateIndex << " " << currentTimeIndex << "\n";
        //}

        // update the total volume with this bar's volume
        totalVolume[currentSymbolIndex][currentDateIndex] += currentOHLCV[4];
            
        int indexGeneral = (currentDateIndex + currentSymbolIndexDay * maxNumDates) * numStoredTimes;

        // if this isn't the first bar recorded for this symbol IN THIS FILE (if it is, no data gets recorded)
        if(lastDateTime[currentSymbolIndex] != DUMMY_INT){
            
            // determine if this bar is the first one past any of the times stored
            for(int i=0;i<numStoredTimes;i++){

                if(currentTime > storedTimes[i] && lastDateTime[currentSymbolIndex] <= storedTimes[i] && priceSweeper[currentSymbolIndex] != DUMMY_INT){
                    // if ascended past, store the ohlcv value as the price sweeper value (last close recorded)
                    p[indexGeneral + i] = priceSweeper[currentSymbolIndex];
                }
            }
        }

        // update the previous datetime recorded for this symbol
        lastDateTime[currentSymbolIndex] = currentDateTime;

        // update the price sweeper for this symbol with the last close recorded
        priceSweeper[currentSymbolIndex] = currentOHLCV[3];

        // if one of the stored times
        if(currentTimeIndex > -1){

            int trueIndex = indexGeneral + currentTimeIndex;

            // store the ohlcv value depending on the storedMetric and which stored time this bar is on
            switch(storedMetrics[currentTimeIndex]){
                case 'O':
                    p[trueIndex] = currentOHLCV[0];
                    break;
                case 'H':
                    p[trueIndex] = currentOHLCV[1];
                    break;
                case 'L':
                    p[trueIndex] = currentOHLCV[2];
                    break;
                case 'C':
                    p[trueIndex] = currentOHLCV[3];
                    break;
                case 'V':
                    p[trueIndex] = currentOHLCV[4];
                    break;
            }
        }
        
        numBarsTotal++;
        numBars[currentSymbolIndex]++;
    }

    f.close();
}

// read every file in the folder given by this address
void readAllFiles(string path, int printLoadingEveryNFiles, bool printLoading) {

    checkColumnCodes();

    checkStoredTimes();

    cout << "Reading files in folder " << path << "... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    numFiles = 0;
    numFiles = size(filesToRead);
    int numFilesComplete = 0, numFilesTotal = 0;
    bool readCustomFiles = numFiles > 0;
    for (const auto& entry : fs::directory_iterator(path)){
        if(!readCustomFiles){
            numFiles++;
        }
        numFilesTotal++;
    }
    for (const auto& entry : fs::directory_iterator(path)){
        fileAddress = entry.path().string();
        if(readCustomFiles){
            for(int i=0;i<numFiles;i++){
                if(filesToRead[i] == fileAddress){
                    readFile();
                    break;
                }
            }
        }else{
            readFile();
        }
        numFilesComplete++;
        if(numFilesComplete % printLoadingEveryNFiles == 0 && printLoading){
            cout << numFilesComplete << "/" << numFiles << " files complete.\n";
        }
    }

    cout << "Finished reading " << numFilesComplete << "/" << numFiles << " files of " << numFilesTotal << " files in this folder.\n\n";
}

// setup the data storage
void setup() {

    cout << "Setting up the program... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    int i = 0, j = 0;

    for (i = 0; i < maxNumSymbols * maxNumDates * numStoredTimes; i++) {
        p[i] = DUMMY_INT;
    }

    numDates = 0;
    numSymbols = 0;
    numAllowedTickers = 0;
    for(int i=0;i<maxNumDates;i++){
        numBarsByDateIndex[i] = 0;
        numFilledByDateIndex[i] = 0;
    }

    for (i = 0; i < maxNumSymbols; i++) {
        numBars[i] = 0;
        priceSweeper[i] = DUMMY_INT;
        lastDateTime[i] = DUMMY_INT;

        for (j = 0; j < maxNumDates; j++) {

            totalVolume[i][j] = 0;
        }

        userTickers[i].clear();
        userVolumes[i] = DUMMY_INT;
        userMarketCaps[i] = DUMMY_INT;
    }
}

void setupDateData(int numVolumeDays, int minVolume, int maxVolume, bool includePartialVolumeSamples) {
    cout << "Setting up the date data storage... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    int i = 0, j = 0, k = 0, l = 0;

    entries = (double**)calloc(numDates, sizeof(double*));
    exits = (double**)calloc(numDates, sizeof(double*));

    long long totalPoints = 0;
    long long filteredPoints = 0;

    // categorize all data points from p[] by date (group into dates)
    for (i = 0; i < numDates; i++) {
        numFilledByDateIndex[i] = 0;

        // use the total number of symbols on this date because it is >= the number of symbols completely filled
        int numPoints = numBarsByDateIndex[i];

        if(numPoints > maxNumSymbols){
            cerr << "The number of data points on day " << i << ", " << numPoints << ", was greater than the maximum number of symbols allowed for any given date, " << maxNumSymbols << ". Increase maxNumSymbols.\n\n";
            exit(1);
        }

        double* entriesDay = (double*)calloc(numPoints, sizeof(double));
        double* exitsDay = (double*)calloc(numPoints, sizeof(double));

        for (j = 0; j < numPoints; j++) {

            totalPoints++;

            // if all points (for different storedTimes) within this symbol and date are in the dataset (!= DUMMY_INT), store the entry and exit for this symbol and day within a temp array and increment # symbols on this day
            bool allPointsTodayFound = 1;
            // location of the point of this date and symbol (first time during day) within p[]
            int index = (i + j * maxNumDates) * numStoredTimes;
            
            for (k = 0; k < numStoredTimes; k++) {
                if (p[index + k] == DUMMY_INT) {
                    allPointsTodayFound = 0;
                }
            }
            if (!allPointsTodayFound) continue;
            
            bool volumeFilter = 1;
            long long vol = 0;
            if(includePartialVolumeSamples || i >= numVolumeDays - 1){
                // only add up previous days' volumes for this symbol if allowed to include volume samples for some but not all of numVolumeDays past days or if i >= numVolumeDays - 1
                int ind = symbol_index.at(index_symbol_day[i].at(j));
                for(k=0;k<numVolumeDays;k++){
                    if(i-k>=0){
                        vol += totalVolume[ind][i-k];
                    }
                }
            }
            if(vol < minVolume || vol > maxVolume) continue;

            filteredPoints++;

            for (k = 0; k < numStoredTimes; k++) {
                if (p[index + k] < 0) {
                    cerr << "Exits[" << i << "][" << numFilledByDateIndex[i] << "] was empty. This must have been because a data value from symbol " << j << " on date " << index_date.at(i) << " was negative.\n\n";
                    exit(1);
                }
            }
                
            // calculation of entries and exits (depends on strategy)
            if(p[index + 0] == 0){
                entriesDay[numFilledByDateIndex[i]] = 0.0;
            }else{
                entriesDay[numFilledByDateIndex[i]] = (double)(p[index + 1] - p[index + 0]) / (double)p[index + 0];
            }
                
            if(p[index + 1] == 0){
                exitsDay[numFilledByDateIndex[i]] = 0.0;
            }else{
                exitsDay[numFilledByDateIndex[i]] = (double)(p[index + 2] - p[index + 1]) / (double)p[index + 1];
            }
            
            index_symbol_sorted[i][numFilledByDateIndex[i]] = index_symbol_day[i].at(j);
            symbol_index_sorted[i][index_symbol_day[i].at(j)] = numFilledByDateIndex[i];
                
            // increment the number of fully-filled symbols on this day
            numFilledByDateIndex[i]++;
        }

        entries[i] = entriesDay;
        exits[i] = exitsDay;
    }
    
    cout << "Filtered " << totalPoints << " data points from " << numDates << " dates down to " << filteredPoints << " data points.\n\n";
}

void sortDateData(bool ascending){
    cout << "Sorting the date data... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    int i = 0, j = 0, k = 0, l = 0;
    bool swap = 0;

    // sort the data within every day of points[]
    double temp = 0.0;
    string temps = "";
    int numBuckets = 10000;
    vector<vector<double>> bucketsEntries(numBuckets, vector<double>(0, 0.0));
    vector<vector<double>> bucketsExits(numBuckets, vector<double>(0, 0.0));
    vector<vector<string>> bucketsSymbols(numBuckets, vector<string>(0, ""));
    double maxVal = (double)INT_MIN;
    double minVal = (double)INT_MAX;
    for (i = 0; i < numDates; i++) {

        int numPoints = numFilledByDateIndex[i];
        if (numPoints == 0) continue;
        
        // get the range of values to initialize the buckets
        for (j = 0; j < numPoints; j++) {

            if (entries[i][j] < minVal) minVal = entries[i][j];
            if (entries[i][j] > maxVal) maxVal = entries[i][j] * 1.0001;
        }
        // clear all buckets from previous use
        for (j = 0; j < numBuckets; j++) {
            bucketsEntries[j].clear();
        }
        double range = maxVal - minVal;
        // if all values are the same, no need to sort (buckets wouldn't work anyway)
        if(range == 0){
            continue;
        }
        int index = 0;
        // fill the buckets with the values to be sorted
        for (j = 0; j < numPoints; j++) {

            index = (int)((double)numBuckets * (entries[i][j] - minVal) / range);
            bucketsEntries[index].push_back(entries[i][j]);
            bucketsExits[index].push_back(exits[i][j]);
            bucketsSymbols[index].push_back(index_symbol_sorted[i][j]);
        }
        // iterate over the buckets
        for (j = 0; j < numBuckets; j++) {
            // sort them using exchange sort
            for (k = 0; k < size(bucketsEntries[j]); k++) {
                for (l = k + 1; l < size(bucketsEntries[j]); l++) {
                    swap = ascending && bucketsEntries[j][k] > bucketsEntries[j][l] || !ascending && bucketsEntries[j][k] < bucketsEntries[j][l];
                    if (swap) {
                        temp = bucketsEntries[j][k];
                        bucketsEntries[j][k] = bucketsEntries[j][l];
                        bucketsEntries[j][l] = temp;

                        temp = bucketsExits[j][k];
                        bucketsExits[j][k] = bucketsExits[j][l];
                        bucketsExits[j][l] = temp;

                        temps = bucketsSymbols[j][k];
                        bucketsSymbols[j][k] = bucketsSymbols[j][l];
                        bucketsSymbols[j][l] = temps;

                    }
                }
            }
        }

        // concatenate the buckets for all data types
        index = 0;
        for (j = 0; j < numBuckets; j++) {
            for (k = 0; k < size(bucketsEntries[j]); k++) {
                entries[i][index] = bucketsEntries[j][k];
                exits[i][index] = bucketsExits[j][k];
                index_symbol_sorted[i][index] = bucketsSymbols[j][k];
                index++;
            }
        }
        if (index != numPoints) {
            cerr << "Some data was unexpectedly lost while sorting values within a given date. Number of bucket values was " << index << " and number of points for this date was " << numPoints << ".\n\n";
            exit(1);
        }

        // assign symbols to indices since we have only sorted indices to symbols on this day
        for(j=0;j<numPoints;j++){
            symbol_index_day[i][index_symbol_sorted[i][j]] = j;
        }
    }
}

void backtest(int startingDateIndex, int endingDateIndex, int numSymbolsPerDay, long long minPreviousVolume, long long maxPreviousVolume, int previousVolumeLookBackLength, double leverage, bool disregardFilters, double minOutlier, double maxOutlier, bool printOutliers, bool ignoreOutliers, bool printAllResults, bool printDetailedResults) {
    cout << "Backtesting with " << numAllowedTickers << " symbols allowed to trade... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";

    if(numDates < 1){
        cerr << "The number of dates found in the dataset was " << numDates << ". It must be at least 1 to backtest.\n\n";
        exit(1);
    }
    if(numSymbols < 1){
        cerr << "The number of ticker symbols found in the dataset was " << numSymbols << ". It must be at least 1 to backtest.\n\n";
        exit(1);
    }
    
    if (startingDateIndex < 0) startingDateIndex = numDates + startingDateIndex;
    if (startingDateIndex < 0) startingDateIndex = 0;
    if (startingDateIndex >= numDates) startingDateIndex = numDates - 1;

    if (endingDateIndex < 0) endingDateIndex = numDates + endingDateIndex;
    if (endingDateIndex < 0) endingDateIndex = 0;
    if (endingDateIndex >= numDates) endingDateIndex = numDates - 1;

    int tradeSymbolIndexByDate = 0;
    long long previousVolume = 0;

    string tradeSymbol = "";

    int numOutliers = 0;

    // outcome stats with respect to # of symbols traded each day
    vector<int> wins(numSymbolsPerDay, 0);
    vector<int> losses(numSymbolsPerDay, 0);
    vector<int> ties(numSymbolsPerDay, 0);
    vector<double> won(numSymbolsPerDay, 0);
    vector<double> lost(numSymbolsPerDay, 0);
    vector<double> balance(numSymbolsPerDay, 1.0);
    vector<double> avSquaredDeviation(numSymbolsPerDay, 1.0);
    vector<double> sd(numSymbolsPerDay, 1.0);

    for (int dateIndex = startingDateIndex; dateIndex <= endingDateIndex; dateIndex++) {

        tradeSymbolIndexByDate = -1;

        for (int i = 0; i < numSymbolsPerDay; i++) {
            
            // find the next symbol that is allowed, quit for this day if all symbols have been checked
            bool flag = 1;
            while (flag) {
                tradeSymbolIndexByDate++;
                if (tradeSymbolIndexByDate >= numFilledByDateIndex[dateIndex]){
                    break;
                }

                previousVolume = 0;
                tradeSymbol = index_symbol_sorted[dateIndex].at(tradeSymbolIndexByDate);
                int tradeSymbolIndex = symbol_index.at(tradeSymbol);

                // add up all previous day volumes if previous days existed
                for (int j = 0; j < previousVolumeLookBackLength; j++) {
                    if (dateIndex - j - 1 >= 0) {
                        previousVolume += totalVolume[symbol_index.at(tradeSymbol)][dateIndex - j - 1];
                    }
                    else {
                        // stop if reaching day 0
                        break;
                    }
                }

                if (previousVolume < minPreviousVolume || previousVolume > maxPreviousVolume) {
                    continue;
                }

                if (disregardFilters) {
                    // trade this symbol rather than continuing onto the next one on this date
                    flag = 0;
                }
                else {

                    flag = 1;

                    // determine if this symbol is allowed via its index. if it is allowed, the loop will end
                    if (tradeSymbolIndex < 0 || tradeSymbolIndex >= numSymbols) {
                        cerr << "Index of symbol to trade (" << tradeSymbolIndex << ") is out of the range of the number of all tickers in the dataset.\n\n";
                        exit(1);
                    }
                    for (int j = 0; j < numAllowedTickers; j++) {
                        if (tradeSymbol == index_allowed.at(j)) {
                            flag = 0;
                            break;
                        }
                    }
                }
            }
        
            // finish quitting for this day if all symbols have been checked
            if (tradeSymbolIndexByDate >= numFilledByDateIndex[dateIndex]) break;

            // actual criteria for entering trade (optional, since already choosing stocks based on entries[] and optional daily volume)
            //

            // trade this symbol
            double result = exits[dateIndex][tradeSymbolIndexByDate];
            if(printAllResults && !printDetailedResults){
                cout << result << " ";
            }
            if(printDetailedResults){
                cout << result << " " << tradeSymbol << " " << index_date.at(dateIndex) << "\n";
            }
            if(result <= minOutlier || result >= maxOutlier){
                numOutliers++;
                if(printOutliers){
                    cout << result << " " << tradeSymbol << " " << index_date.at(dateIndex) << "\n";
                }
                if(ignoreOutliers){
                    result = 0.0;
                }
            }
            if (result > 0.0) {
                for(int j=i;j<numSymbolsPerDay;j++){
                    wins[j]++;
                    won[j] += result;
                }
            }
            else {
                if (result < 0.0) {
                    for(int j=i;j<numSymbolsPerDay;j++){
                        losses[j]++;
                        lost[j] += result;
                    }
                }
                else {
                    for(int j=i;j<numSymbolsPerDay;j++){
                        ties[j]++;
                    }
                }
            }

            // update account balance stats
            for(int j=i;j<numSymbolsPerDay;j++){
                avSquaredDeviation[j] += result * result;
                balance[j] *= (1.0 + result);
            }
        }
    }

    string output = "";

    output += "Backtested from date with index " + to_string(startingDateIndex) + " to index " + to_string(endingDateIndex) + ", " + to_string(endingDateIndex - startingDateIndex + 1) + " days.\n\n";
    
    output += "ANALYSIS:\n\nTrades: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(wins[j] + losses[j] + ties[j]) + "   ";
    }
    
    output += "\nWins: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(wins[j]) + "   ";
    }
    
    output += "\nLosses: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(losses[j]) + "   ";
    }
    
    output += "\nTies: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(ties[j]) + "   ";
    }

    output += "\nPercentage Won: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(won[j] * 100.0) + "   ";
    }

    output += "\nPercentage Lost: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(lost[j] * 100.0) + "   ";
    }

    output += "\nTotal Percentage Gained: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string((won[j] + lost[j]) * 100.0) + "   ";
    }

    output += "\nProfit Factor: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(-1.0 * won[j] / lost[j]) + "   ";
    }

    output += "\nFinal Balance: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string(balance[j]) + "   ";
    }

    output += "\nVariance of Trade Results From Zero: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        avSquaredDeviation[j] /= (double)(wins[j] + losses[j]);
        output += to_string(avSquaredDeviation[j]) + "   ";
    }

    output += "\nStandard Deviation of Trade Results From Zero: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        sd[j] = sqrt(avSquaredDeviation[j]);
        output += to_string(sd[j]) + "   ";
    }

    output += "\nSharpe Ratio: ";
    for(int j=0;j<numSymbolsPerDay;j++){
        output += to_string((won[j] + lost[j]) / sd[j]) + "   ";
    }

    output += "\n\nThere were " + to_string(numOutliers) + " outlying trade results.\n\n";

    cout << output;
}

// find start of first occurrence of word in s
int findInString(string s, string word) {
    int n = size(s);
    int w = size(word);
    bool in = 1;
    int m = n - w + 1;
    for (int i = 0; i < m; i++) {
        in = 1;
        for (int j = 0; j < w; j++) {
            if (s[i + j] != word[j]) {
                in = 0;
                break;
            }
        }
        if (in) return i;
    }
    return -1;
}

// replace all occurrences of from with to
string replace(string s, string from, string to) {
    int n = size(s);
    string o = "";
    int f = size(from);
    int t = size(to);
    bool match = 1;
    for (int i = 0; i < n; i++) {
        match = 1;
        for (int j = 0; j < f; j++) {
            if (i + j >= n) break;
            if (s[i + j] != from[j]) {
                match = 0; break;
            }
        }
        if (match) {
            o += to;
        }
        else {
            o += s[i];
        }
    }
    return o;
}

// format a user-inputted list of symbols for easy downloading/organization. from and to: replace occurrences, start and end: placed at ends of formatted string.
string formatSymbolList(string list, string from, string to, string start, string end) {
    string s = replace(list, from, to);
    return start + s + end;
}

// helper function for extractSymbolData
string extractTicker(vector<char> s, int i, int spot) {
    int l = size(s);
    string o = "";
    while (spot < l) {
        if ((s[spot] < 'a' || s[spot] > 'z') && (s[spot] < 'A' || s[spot] > 'Z') && s[spot] != '.') {
            return o;
        }
        o += s[spot];
        spot++;
    }
    return o;
}

// helper function for extractSymbolData
long long extractNumber(vector<char> s, int i, int numDecimalPrecisionDigits, int spot) {
    int l = size(s);
    int subSpot = spot + 1;
    if (!isNumeric(s[spot])) {
        return DUMMY_INT;
    }
    while (s[subSpot] != ',' && s[subSpot] != '.') {
        subSpot++;
        if (subSpot >= l) {
            cerr << "Number overflowed size of string while extracting symbol data.\n\n";
            exit(1);
        }
    }
    long long num = 0;
    // get digits before .
    for (int i = spot; i < subSpot; i++) {
        num += (long long)((int)s[i] - '0') * power(10, subSpot - i - 1 + numDecimalPrecisionDigits);
    }
    // get digits after .
    if (s[subSpot] == '.') {
        subSpot++;
        for (int i = 0; i < numDecimalPrecisionDigits; i++) {
            if (subSpot + i >= l) {
                cerr << "Number overflowed size of string while extracting symbol data.\n\n";
                exit(1);
            }
            if (!isNumeric(s[subSpot + i])) break;
            num += (long long)((int)s[subSpot + i] - '0') * power(10, numDecimalPrecisionDigits - i - 1);
        }
    }
    return num;
}

// get symbol/volume/market cap info from a file, store in global arrays. margin: # of chars between "preceding" strings and data strings. numDecimalPrecisionDigits: # of decimal digits to record (negative prevents overflows)
void extractSymbolData(string address, string precedingSymbol, string precedingVolume, string precedingMarketCap, int margin, int numDecimalPrecisionDigitsVolume, int numDecimalPrecisionDigitsMarketCap) {
    cout << "Extracting symbol data from " << address << "...\n\n";

    if (margin < 0 || margin > 100) {
        cerr << "Margin must be a value from 0 to 100.\n\n";
        exit(1);
    }
    if (numDecimalPrecisionDigitsVolume < -10 || numDecimalPrecisionDigitsVolume > 10) {
        cerr << "numDecimalPrecisionDigitsVolume must be a value from -10 to 10.\n\n";
        exit(1);
    }
    if (numDecimalPrecisionDigitsMarketCap < -10 || numDecimalPrecisionDigitsMarketCap > 10) {
        cerr << "numDecimalPrecisionDigitsMarketCap must be a value from -10 to 10.\n\n";
        exit(1);
    }

    ifstream f(address);

    if (!f.is_open()) {
        cerr << "Symbol data file could not open. Make sure the file address is correct.\n\n";
        exit(1);
    }

    // read all lines from address into symbolData
    string line = "";
    while (getline(f, line)) {
        for(int i=0;i<size(line);i++){
            symbolData.push_back(line[i]);
        }
    }

    int l = size(symbolData);
    int ps = size(precedingSymbol);
    int pv = size(precedingVolume);
    int pm = size(precedingMarketCap);
    int i = 0, j = 0, symbol = -1;
    bool in = 1;
    for (i = 0; i < l; i++) {
        in = 1;
        // is the precedingSymbol string here?
        for (j = 0; j < ps; j++) {
            if (i + j < l) {
                if (symbolData[i + j] != precedingSymbol[j]) {
                    in = 0;
                    break;
                }
            }
            else {
                break;
            }
        }
        // extract the ticker margin chars past the end of precedingSymbol
        if (in) {
            symbol++;
            if (symbol < 0 || symbol >= maxNumSymbols) {
                cerr << "Symbol " << symbol << " for a ticker went out of range [0, " << maxNumSymbols << "] when extracting symbol data.\n\n";
                exit(1);
            }
            else {
                userTickers[symbol] = extractTicker(symbolData, i, i + ps + margin);
            }
        }
        in = 1;
        // is the precedingVolume string here?
        for (j = 0; j < pv; j++) {
            if (i + j < l) {
                if (symbolData[i + j] != precedingVolume[j]) {
                    in = 0;
                    break;
                }
            }
            else {
                break;
            }
        }
        // extract the number margin chars past the end of precedingVolume
        if (in) {
            if (symbol < 0 || symbol >= maxNumSymbols) {
                cerr << "Symbol " << symbol << " for a volume went out of range [0, " << maxNumSymbols << "] when extracting symbol data.\n\n";
                exit(1);
            }
            else {
                userVolumes[symbol] = extractNumber(symbolData, i, numDecimalPrecisionDigitsVolume, i + pv + margin);
            }
        }
        in = 1;
        // is the precedingMarketCap string here?
        for (j = 0; j < pm; j++) {
            if (i + j < l) {
                if (symbolData[i + j] != precedingMarketCap[j]) {
                    in = 0;
                    break;
                }
            }
            else {
                break;
            }
        }
        // extract the number margin chars past the end of precedingMarketCap
        if (in) {
            if (symbol < 0 || symbol >= maxNumSymbols) {
                cerr << "Symbol " << symbol << " for a market cap went out of range [0, " << maxNumSymbols << "] when extracting symbol data.\n\n";
                exit(1);
            }
            else {
                userMarketCaps[symbol] = extractNumber(symbolData, i, numDecimalPrecisionDigitsMarketCap, i + pm + margin);
            }
        }
    }
}

// filters the symbols used to backtest that are stored in userTickers, userVolumes, and userMarketCaps by volume and market cap
void filterSymbols(long long minVolume, long long maxVolume, long long minMarketCap, long long maxMarketCap, vector<string> listOfBannedTickers) {
    cout << "Filtering the user-provided symbols by volume and market cap...\n\n";
    numAllowedTickers = 0;
    for (int i = 0; i < maxNumSymbols; i++) {
        if (size(userTickers[i]) > 0 && userVolumes[i] >= minVolume && userVolumes[i] <= maxVolume && userMarketCaps[i] >= minMarketCap && userMarketCaps[i] <= maxMarketCap) {
            allowed_index[userTickers[i]] = numAllowedTickers;
            index_allowed[numAllowedTickers] = userTickers[i];
            numAllowedTickers++;
        }
    }
}

void printUserSymbols(bool printVolumeAndMarketCap) {
    cout << "Printing all user-defined symbols.\n";
    int total = 0;
    for (int i = 0; i < maxNumSymbols; i++) {
        if (size(userTickers[i]) > 0) {
            total++;
            for (int j = 0; j < size(userTickers[i]); j++) {
                cout << userTickers[i][j];
            }
            if (printVolumeAndMarketCap) {
                cout << " " << userVolumes[i] << " " << userMarketCaps[i] << "\n";
            }
            else {
                cout << " ";
            }
        }
    }
    cout << "\nPrinted " << total << " user-defined symbols.\n\n";
}

void printAllowedSymbols(bool printVolumeAndMarketCap) {
    cout << "Printing " << numAllowedTickers << " allowed symbols.\n";
    for (int i = 0; i < numAllowedTickers; i++) {
        for (int j = 0; j < size(index_allowed[i]); j++) {
            cout << index_allowed[i][j];
        }
        if (printVolumeAndMarketCap) {
            cout << " " << userVolumes[symbol_index.at(index_allowed.at(i))] << " " << userMarketCaps[symbol_index.at(index_allowed.at(i))] << "\n";
        }
        else {
            cout << " ";
        }
    }
    cout << "\nPrinted " << numAllowedTickers << " allowed symbols.\n\n";
}

int main() {
    setup();

    string readPath = "C:\\Minute Stock Data\\";
    columnCodes = "SVOCHLU-";
    // filesToRead = {"C:\\Minute Stock Data\\2025-05-12.csv"};
    readAllFiles(readPath, 5, true);

    setupDateData(10, 1000000, INT_MAX, 1);
    sortDateData(true);

    vector<string> banned = { "" };
    //extractSymbolData("C:\\Stock Symbols\\symbolDataNASDAQ.txt", "symbol:", "volume:", "marketCap:", 1, 0, -6);
    //filterSymbols(0, LLONG_MAX, 0, LLONG_MAX, banned);
    //printAllowedSymbols(false);
    ////    Comment out either the above three lines or the line below this to use
    backtest(0, -1, 1, 0, INT_MAX, 1, 0.1, true, -0.5, 0.5, true, true, true, true);
    
    return 0;
}

/*

EXAMPLE 1 OF COLUMN CODE FORMAT FOR AN EXCERPT FROM A DATA FILE:

moment, unused, unused, unused, open, high, low, close, volume, symbol
M---OHLCVS

ts_event,rtype,publisher_id,instrument_id,open,high,low,close,volume,symbol
2025-03-03T09:00:00.000000000Z,33,2,11667,124.560000000,124.750000000,124.390000000,124.490000000,3499,NVDA
2025-03-03T09:00:00.000000000Z,33,2,38,241.420000000,241.790000000,241.130000000,241.480000000,1508,AAPL
2025-03-03T09:00:00.000000000Z,33,2,853,201.990000000,213.060000000,201.990000000,212.750000000,1067,AMZN
2025-03-03T09:01:00.000000000Z,33,2,853,212.750000000,213.080000000,212.750000000,213.080000000,1139,AMZN
2025-03-03T09:01:00.000000000Z,33,2,11667,124.490000000,124.920000000,124.490000000,124.910000000,11068,NVDA
2025-03-03T09:01:00.000000000Z,33,2,38,241.480000000,241.650000000,241.410000000,241.500000000,1233,AAPL
2025-03-03T09:02:00.000000000Z,33,2,38,241.500000000,241.550000000,241.270000000,241.270000000,24,AAPL
2025-03-03T09:02:00.000000000Z,33,2,853,213.230000000,213.260000000,213.000000000,213.000000000,2103,AMZN
2025-03-03T09:02:00.000000000Z,33,2,11667,124.920000000,124.960000000,124.710000000,124.750000000,5581,NVDA
2025-03-03T09:03:00.000000000Z,33,2,38,241.450000000,241.600000000,241.350000000,241.600000000,932,AAPL



EXAMPLE 2 OF COLUMN CODE FORMAT FOR AN EXCERPT FROM A DATA FILE:

symbol, volume, open, close, high, low, unix_time, unused
SVOCHLU-

ticker,volume,open,close,high,low,window_start,transactions
A,699,109.21,109.64,109.64,109.21,1747053840000000000,15
A,31859,110.81,111.27,111.665,110.45,1747056600000000000,336
A,26947,110.78,111.2,111.2,110.7,1747056660000000000,138
A,7057,111.125,112.46,112.46,111.12,1747056720000000000,141
A,13947,112.46,112.5025,112.68,112.41,1747056780000000000,182
A,20491,112.5,112.93,112.99,112.42,1747056840000000000,258
A,7357,112.86,113.425,113.425,112.85,1747056900000000000,150
A,19706,113.52,113.965,114.18,113.39,1747056960000000000,395
A,6082,114.04,113.965,114.05,113.4901,1747057020000000000,119
A,5756,113.93,113.66,113.93,113.66,1747057080000000000,88

*/
