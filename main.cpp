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

int numFiles = 0;
int numFilesComplete = 0;

// data used to read CSV files
vector<string> filesToRead;
string fileAddress = "";
string fileLine = "";
int fileLineIndex = 0;
int fileLineLength = 0;
int fileLineSpot = 0;
int fileLineSpot1 = 0;

// date indices that have not been filled
#define DUMMY_INT INT_MIN
// empty prices, entries[] and exits[] values
#define DUMMY_DOUBLE (double)INT_MIN


// times whose bars should be stored and used to backtest
vector<int> storedTimes = { 930, 935, 1500 };
// metrics (OHLCV) of the bars that should be stored and used to backtest
vector<int> storedMetrics = { 'O', 'C', 'C' };

string columnCodes = "";

// daywise backtesting option - allows sampling data from certain times in every day and trading with that data rather than trading based on intraday patterns
bool daywiseBacktesting = 0;

#define maxNumSymbols 20000 // with about 12 arrays of size maxNumSymbols * maxNumDates, this product can be at most ~10000000 (resulting in 960M bytes)
#define maxNumDates 400
#define numStoredTimes 3

#define numMinutesPerDay 390 // 60m * 6.5h

double open[maxNumSymbols][numMinutesPerDay];
double high[maxNumSymbols][numMinutesPerDay];
double low[maxNumSymbols][numMinutesPerDay];
double close[maxNumSymbols][numMinutesPerDay];
double volume[maxNumSymbols][numMinutesPerDay];

// number of possible values for the variables when minutewise backtesting
#define mwbtNumTrialsFirst 4
#define mwbtNumTrialsSecond 4
#define mwbtNumTrialsThird 5

const int mwbtNumTrialsTotal = mwbtNumTrialsFirst * mwbtNumTrialsSecond * mwbtNumTrialsThird;

// trade entry price and time for each strategy, dummy when not in trade
double entryPrices[maxNumSymbols][mwbtNumTrialsTotal];
int entryMinutesSinceOpen[maxNumSymbols][mwbtNumTrialsTotal];

// file parsing info
long long numBarsTotal = 0;
int numBars[maxNumSymbols];
int currentSymbolIndex = 0;
string currentSymbol = "";
long long currentDateTime = 0;
int currentDate = 0;
int currentDateIndex = 0;
int currentTime = 0;
int currentTimeIndex = 0;
vector<double> currentOHLCV = {0.0,0.0,0.0,0.0,0.0};


// data points at storedTimes for daywise backtesting
double*** p;


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

// minutewise basic measurements
long long lastDateTime[maxNumSymbols];
double lastPrice[maxNumSymbols];
int earliestTimeDay[maxNumSymbols][maxNumDates];
double dailyOpen[maxNumSymbols][maxNumDates];
double dailyClose[maxNumSymbols][maxNumDates];
double dailyHigh[maxNumSymbols][maxNumDates];
double dailyLow[maxNumSymbols][maxNumDates];
double dailyHighTime[maxNumSymbols][maxNumDates];
double dailyLowTime[maxNumSymbols][maxNumDates];
double dailyRange[maxNumSymbols][maxNumDates];
double currentDailyRangeIndex[maxNumSymbols];
double dailyChange[maxNumSymbols][maxNumDates];
double dailyGap[maxNumSymbols][maxNumDates];
double dailyChangePortion[maxNumSymbols][maxNumDates];
double dailyGapPortion[maxNumSymbols][maxNumDates];

// cumulative volume for each symbol and date (used in both daywise and minutewise backtesting)
long long totalVolume[maxNumSymbols][maxNumDates];

#define maxMALength 101

double ma[maxNumSymbols][maxMALength];


// predefined user-created symbol information for user symbol organization and volume/market cap filtering during backtesting
string userTickers[maxNumSymbols];
long long userVolumes[maxNumSymbols];
long long userMarketCaps[maxNumSymbols];

// symbols allowed to trade. filter by volume and market cap using data from the above arrays.
int numAllowedTickers = 0;
unordered_map<string, int> allowed_index;
unordered_map<int, string> index_allowed;

// basic minutewise measurements
double cumulativeVolume[numMinutesPerDay];
long long tv = 0;
double change[numMinutesPerDay];
int numGreen[numMinutesPerDay];
int numRed[numMinutesPerDay];
int numUp[numMinutesPerDay];
int numDown[numMinutesPerDay];
int ng = 0, nr = 0, nu = 0, nd = 0;

vector<int> tradingTimes = {0, numMinutesPerDay - 1};

// global backtesting settings
int btStartingDateIndex = 0;
int btEndingDateIndex = 0;
int dwbtNumSymbolsPerDay = 0;
long long dwbtMinPreviousVolume = 0;
long long dwbtMaxPreviousVolume = 0;
int btPreviousVolumeLookBackLength = 0;
int mwbtEarliestTimeToTrade = 930;
int mwbtLatestTimeToTrade = 1559;
double btLeverage = 1.0;
bool btDisregardFilters = false;
double btMinOutlier = 0.0;
double btMaxOutlier = 0.0;
bool btIgnoreOutliers = true;
bool btPrintOutliers = true;
bool btPrintEntries = true;
bool btPrintAllResults = true;
bool btPrintDetailedResults = true;
bool btPrintLoading = true;
int btPrintLoadingInterval = 1;
int btLoadingBarWidth = 60;
bool btPrintSummary = true;

// outcome stats with respect to # of symbols traded each day
double btResult = 0.0;

// daywise backtesting stats for different numbers of symbols traded per day (dwbtNumSymbolsPerDay)
vector<int> dwbtWins;
vector<int> dwbtLosses;
vector<int> dwbtTies;
vector<double> dwbtWon;
vector<double> dwbtLost;
vector<double> dwbtBalance;
vector<double> dwbtVar;
vector<double> dwbtSD;
vector<int> dwbtNumOutliers;

// minutewise backtesting stats for different strategies
vector<int> mwbtWins;
vector<int> mwbtLosses;
vector<int> mwbtTies;
vector<double> mwbtWon;
vector<double> mwbtLost;
vector<double> mwbtBalance;
vector<double> mwbtVar;
vector<double> mwbtSD;
vector<int> mwbtNumOutliers;

string aggregateBacktestingResults = "";


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
    bool leapDay = 0;
    if(year % 4 == 0){ // if it's a leap year (100-year rule does not matter because 2000 is a leap year and 1900/2100 aren't here)
        if(dayOfYear == 59){
            month = 2;
            dateTime += 229;
            dayOfMonth = 29;
            leapDay = 1;
        }else{
            if(dayOfYear > 59){
                dayOfYear--;
            }
        }
    }

    if(!leapDay){
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
double parseOHLCV(char ohlcvCode){
    
    while (fileLine[fileLineSpot1] != ',' && fileLine[fileLineSpot1] != '.') {
        fileLineSpot1++;
        if (fileLineSpot1 >= fileLineLength) {
            break;
        }
    }
    double num = 0.0;
    // get digits before .
    for (int j = fileLineSpot; j < fileLineSpot1; j++) {
        num += (double)((fileLine[j] - '0') * (int)power(10, fileLineSpot1 - j - 1));
    }
    if (ohlcvCode == 'V') {
        return num;
    }
    
    // get all digits after .
    if (fileLine[fileLineSpot1] == '.') {
        double power = 0.1;
        while(1){
            fileLineSpot1++;
            if(fileLineSpot1 >= fileLineLength) {
            return num;}
            if(!isNumeric(fileLine[fileLineSpot1])){
            return num;}
            num += (double)(fileLine[fileLineSpot1] - '0') * power;
            power *= 0.1;
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

// helper function for reading datetimes
void addNewDateTime(){
    if (date_index.find(currentDate) != date_index.end()) {
        currentDateIndex = date_index.at(currentDate);
    }else{
        currentDateIndex = numDates;
        date_index[currentDate] = numDates;
        index_date[numDates] = currentDate;
        numDates++;
        if(numDates > maxNumDates){
            cerr << "The number of dates was greater than the maximum number of dates allowed, " << maxNumDates << ". Increase maxNumDates.\n\n";
            exit(1);
        }
    }
}

// convert minutes past 930 to time
int index_time(int i){
    if(i < 0 || i > 599){
        cerr << "Number of minutes past open must be between 0 and 599. It is " << i << ".\n";
        exit(1);
    }
    i += 30;
    return ((i / 60) + 9) * 100 + (i % 60);
}

// convert time to minutes past 930
int time_index(int t){
    if(t < 930 || t > 2359){
        cerr << "Time must be between 930 and 2359. It is " << t << ".\n";
        exit(1);
    }
    if(t % 100 > 59){
        cerr << "Minute component of time must be between 0 and 59. It is " << t << ".\n";
        exit(1);
    }
    return ((t % 100) - 30) + ((t / 100) - 9) * 60;
}

void simulateMinute(int s, int d, int t, int firstTrial, int secondTrial, int thirdTrial, int trial){
    string symbol = index_symbol.at(s);
    int T = index_time(t);

    int minutesSinceEntry = -1;
    if(entryMinutesSinceOpen[s][trial] != DUMMY_INT){
        minutesSinceEntry = t - entryMinutesSinceOpen[s][trial];
    };

    // exit condition logic
//bool exit = minutesSinceEntry >= 30;
bool exit = T == 1559;

    // exit a trade at this minute if currently in a trade and exit condition succeeds (varies based on strategy)
    // NOTE: we can only exit on a listed bar. so if exits are time-based rather than price-based, they may be unideal and even biased
exit &= entryMinutesSinceOpen[s][trial] != DUMMY_INT && entryPrices[s][trial] != DUMMY_DOUBLE;
    if(exit){

        double entryPrice = entryPrices[s][trial];
        double exitPrice = close[s][t];

        btResult = ((exitPrice - entryPrice) / entryPrice) * btLeverage;
        if(btPrintAllResults && !btPrintDetailedResults && !btPrintLoading){
            cout << btResult << " ";
        }
        if(btPrintDetailedResults && !btPrintLoading){
            cout << "Exit " << firstTrial << " " << secondTrial << " " << thirdTrial << ": " << exitPrice << " " << btResult << " " << symbol << " " << index_date.at(d) << " " << index_time(entryMinutesSinceOpen[s][trial]) << "-" << index_time(t) << "\n";
        }
        if(btResult <= btMinOutlier || btResult >= btMaxOutlier){
            mwbtNumOutliers[trial]++;
            if(btPrintOutliers && !btPrintLoading){
                cout << "Outlier " << firstTrial << " " << secondTrial << " " << thirdTrial << ": " << btResult << " " << symbol << " " << index_date.at(d) << " " << index_time(entryMinutesSinceOpen[s][trial]) << "\n";
            }
            if(btIgnoreOutliers){
                btResult = 0.0;
            }
        }
        if (btResult > 0.0) {
            mwbtWins[trial]++;
            mwbtWon[trial] += btResult;
        }
        else {
            if (btResult < 0.0) {
                mwbtLosses[trial]++;
                mwbtLost[trial] += btResult;
            }
            else {
                mwbtTies[trial]++;
            }
        }

        // update account balance stats
        mwbtVar[trial] += btResult * btResult;
        mwbtBalance[trial] *= (1.0 + btResult);

        entryPrices[s][trial] = DUMMY_DOUBLE;
        entryMinutesSinceOpen[s][trial] = DUMMY_INT;
    }
    
    // entry condition logic
//bool enter = numGreen[t] == t + 1;
bool enter = dailyGapPortion[s][d] != DUMMY_DOUBLE && dailyGapPortion[s][d] >= 0.0 + ((double)secondTrial * 0.02);
if(d > 0){
    enter &= totalVolume[s][d - 1] >= firstTrial * 2000000 + 10000000; // volume filtering using previous day's volume
}else{enter = 0;}
enter &= change[t] != DUMMY_DOUBLE && change[t] >= 0.0 + ((double)thirdTrial * 0.02);
//if(d > 0){
//    enter &= totalVolume[s][d - 1] * (long long)close[s][t] >= (long long)firstTrial * 20000000ll + 100000000ll; // volume filtering using previous day's turnover
//}else{enter = 0;}
//enter &= cumulativeVolume[t] / (double)(t + 1) >= 5000.0; // volume filtering using vpm
enter &= T == 930; // time of day filtering
enter &= close[s][t] >= 1.0; // price filtering
enter &= entryMinutesSinceOpen[s][trial] == DUMMY_INT && entryPrices[s][trial] == DUMMY_DOUBLE;
// enter &= d >= btStartingDateIndex && d <= btEndingDateIndex; can't check date indices bc dk how many dates there are while reading
    if(enter){

        entryPrices[s][trial] = close[s][t];
        entryMinutesSinceOpen[s][trial] = t;

        if(btPrintEntries && !btPrintLoading){
            cout << "Entry " << firstTrial << " " << secondTrial << " " << thirdTrial << ": " << entryPrices[s][trial] << " " << symbol << " " << index_date.at(d) << " " << index_time(entryMinutesSinceOpen[s][trial]) << "\n";
        }
    }
}

// exit and enter at specified times during the day currently recorded in ohlcv arrays
void simulateTradesMinutewise(int s, int d){
        
    // fill in holes
    for(int t=1;t<numMinutesPerDay;t++){
        if(close[s][t] == DUMMY_DOUBLE){
            open[s][t] = close[s][t-1];
            high[s][t] = close[s][t-1];
            low[s][t] = close[s][t-1];
            close[s][t] = close[s][t-1];
            volume[s][t] = 0;
        }
    }

    // update dailies for fully filled day
    
    dailyOpen[s][d] = open[s][0];
    dailyClose[s][d] = close[s][numMinutesPerDay - 1];

    dailyHigh[s][d] = -1.0 * 1e20;
    dailyLow[s][d] = 1.0 * 1e20;
    for(int t=0;t<numMinutesPerDay;t++){
        if(high[s][t] > dailyHigh[s][d]){
            dailyHigh[s][d] = high[s][t];
            dailyHighTime[s][d] = currentTime;
        }
        if(low[s][t] < dailyLow[s][d]){
            dailyLow[s][d] = low[s][t];
            dailyLowTime[s][d] = currentTime;
        }
    }
    
    dailyChange[s][d] = dailyClose[s][d] - dailyOpen[s][d];
    if(dailyOpen[s][d] == 0.0){ // avoid / by 0
        dailyChangePortion[s][d] = 0.0;
    }else{
        dailyChangePortion[s][d] = dailyChange[s][d] / dailyOpen[s][d];
    }
    if(d > 0){
        if(dailyClose[s][d - 1] != DUMMY_DOUBLE){
            // if previous day exists and had a recorded close, record gap
            dailyGap[s][d] = dailyOpen[s][d] - dailyClose[s][d - 1];
            if(dailyClose[s][d - 1] == 0.0){ // avoid / by 0
                dailyGapPortion[s][d] = 0.0;
            }else{
                dailyGapPortion[s][d] = dailyGap[s][d] / dailyClose[s][d - 1];
            }
        }
    }
    dailyRange[s][d] = dailyHigh[s][d] - dailyLow[s][d];
    if(dailyRange[s][d] == 0.0){ // avoid / by 0
        currentDailyRangeIndex[s] = 0.5;
    }else{
        currentDailyRangeIndex[s] = (dailyClose[s][d] - dailyLow[s][d]) / dailyRange[s][d];
    }
    
    // BASIC MEASUREMENTS

    ng = 0; nr = 0; nu = 0; nd = 0;
    for(int m=0;m<numMinutesPerDay;m++){
        tv += volume[s][m];
        cumulativeVolume[m] = tv;
        if(open[s][0] == 0.0){
            change[m] = 0.0;
        }else{
            change[m] = (close[s][m] - open[s][0]) / open[s][0];
        }
        if(close[s][m] > open[s][m]){
            ng++;
        }
        if(close[s][m] < open[s][m]){
            nr++;
        }
        if(m > 0){
            if(close[s][m] > close[s][m - 1]){
                nu++;
            }
            if(close[s][m] < close[s][m - 1]){
                nd++;
            }
        }
        numGreen[m] = ng;
        numRed[m] = nr;
        numUp[m] = nu;
        numDown[m] = nd;
    }

    // update the moving averages
    ma[currentSymbolIndex][currentDateIndex] = 0.0;
    //for(int i=0;i<)

    
    // execute all exits and entries on all trials for this symbol today
    //for(int t = time_index(mwbtEarliestTimeToTrade); t <= time_index(mwbtLatestTimeToTrade); t++){
    for(int t: tradingTimes){
    for(int firstTrial = 0; firstTrial < mwbtNumTrialsFirst; firstTrial++){
    for(int secondTrial = 0; secondTrial < mwbtNumTrialsSecond; secondTrial++){
    for(int thirdTrial = 0; thirdTrial < mwbtNumTrialsThird; thirdTrial++){

        int trial = firstTrial * mwbtNumTrialsSecond * mwbtNumTrialsThird + secondTrial * mwbtNumTrialsThird + thirdTrial;
        simulateMinute(s, d, t, firstTrial, secondTrial, thirdTrial, trial);

    }}}}
    
    // clear
    for(int t=0;t<numMinutesPerDay;t++){
        open[s][t] = DUMMY_DOUBLE;
        high[s][t] = DUMMY_DOUBLE;
        low[s][t] = DUMMY_DOUBLE;
        close[s][t] = DUMMY_DOUBLE;
        volume[s][t] = DUMMY_DOUBLE;
    }
}

void updateAfterBar(){
    // update the last price for this symbol with the last close recorded
    lastPrice[currentSymbolIndex] = currentOHLCV[3];

    // update the previous datetime recorded for this symbol
    lastDateTime[currentSymbolIndex] = currentDateTime;
        
    numBarsTotal++;
    numBars[currentSymbolIndex]++;
}

void recordPointDaywise(){
    int currentSymbolIndexDay = symbol_index_day[currentDateIndex].at(currentSymbol);
    double* P = p[currentDateIndex][currentSymbolIndexDay];

    // if this isn't the first bar recorded for this symbol EVER (if it is, data cannot be recorded)
    if(numBars[currentSymbolIndex] > 0){
                
            // determine if this bar is the first one past any of the times stored
        for(int i=0;i<numStoredTimes;i++){

            if(currentTime > storedTimes[i] && lastDateTime[currentSymbolIndex] < storedTimes[i] && lastPrice[currentSymbolIndex] != DUMMY_DOUBLE){
                // if ascended past, store the price sweeper value (last close recorded) as the ohlcv value
                P[i] = lastPrice[currentSymbolIndex];

                // may handle repeated sweeping recordings error here (like below for exact stored times) but it doesn't matter really
            }
        }
    }

    // if one of the stored times
    if(currentTimeIndex > -1){

        if(P[currentTimeIndex] != DUMMY_DOUBLE){
                    
            cout << "Data point with date index " << to_string(currentDateIndex);
            cout << " and symbol index on that day " << to_string(currentSymbolIndexDay);
            cout << " (symbol " << index_symbol_day[currentDateIndex].at(currentSymbolIndexDay) << currentSymbol;
            cout << ") and time location index " << to_string(currentTimeIndex);
            cout << " was recorded twice.\n";
            cout << "The first recorded value was " << to_string(P[currentTimeIndex]);
            cout << " and the second recorded value was ";
            switch(storedMetrics[currentTimeIndex]){
            case 'O':
                cout << currentOHLCV[0];
                break;
            case 'H':
                cout << currentOHLCV[1];
                break;
            case 'L':
                cout << currentOHLCV[2];
                break;
            case 'C':
                cout << currentOHLCV[3];
                break;
            case 'V':
                cout << currentOHLCV[4];
                break;
            default:
                cout << "nonexistent";
                break;
            }
            cerr << ".\n";
            exit(1); 
        }

        // store the ohlcv value depending on the storedMetric and which stored time this bar is on
        switch(storedMetrics[currentTimeIndex]){
            case 'O':
                P[currentTimeIndex] = currentOHLCV[0];
                break;
            case 'H':
                P[currentTimeIndex] = currentOHLCV[1];
                break;
            case 'L':
                P[currentTimeIndex] = currentOHLCV[2];
                break;
            case 'C':
                P[currentTimeIndex] = currentOHLCV[3];
                break;
            case 'V':
                P[currentTimeIndex] = currentOHLCV[4];
                break;
        }
    }
}

// read one line
void readLine(int numValuesPerLine){
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
                if(numSymbols > maxNumSymbols){
                    cerr << "The number of symbols was greater than the maximum number of symbols allowed, " << maxNumSymbols << ". Increase maxNumSymbols.\n\n";
                    exit(1);
                }
            }
            break;
        }
        case 'M':{
            currentDateTime = parseMoment();
            currentDate = currentDateTime / 10000;
            currentTime = currentDateTime % 10000;
            currentTimeIndex = -1;
            addNewDateTime();
            for(int i=0;i<numStoredTimes;i++){
                if(storedTimes[i] == currentTime){
                    currentTimeIndex = i;
                    break;
                }
            }
            break;
        }
        case 'U':{
            currentDateTime = parseUnix();
            currentDate = currentDateTime / 10000;
            currentTime = currentDateTime % 10000;
            currentTimeIndex = -1;
            addNewDateTime();
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
            addNewDateTime();
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

    // if this isn't the first bar recorded of this symbol EVER (if it is, ascend/descend does not matter as there is no previous datetime to compare to)
    if(numBars[currentSymbolIndex] > 0){

        // handle out of ascending order exception
        if (currentDateTime <= lastDateTime[currentSymbolIndex]) {
            cerr << "Consecutively parsed bars " << lastDateTime[currentSymbolIndex] << " and " << currentDateTime << " of symbol " << index_symbol.at(currentSymbolIndex) << " were not recorded in ascending order as previous bars of this symbol and/or other symbols were.\n\n";
            exit(1);
        }
    }

    // update the map of symbols on this day if a new symbol is found
    int currentSymbolIndexDay = 0;
    if (symbol_index_day[currentDateIndex].find(currentSymbol) != symbol_index_day[currentDateIndex].end()) {
        currentSymbolIndexDay = symbol_index_day[currentDateIndex].at(currentSymbol);
    }else{
        currentSymbolIndexDay = numBarsByDateIndex[currentDateIndex];
        symbol_index_day[currentDateIndex][currentSymbol] = currentSymbolIndexDay;
        index_symbol_day[currentDateIndex][currentSymbolIndexDay] = currentSymbol;
        numBarsByDateIndex[currentDateIndex]++;
    }

    // update the total volume with this bar's volume
    totalVolume[currentSymbolIndex][currentDateIndex] += (long long)currentOHLCV[4];

    // check if all previous minutes have been recorded/filled
    bool fullyFilled = 1;
    for(int i=0;i<numMinutesPerDay;i++){
        if(open[currentSymbolIndex][i] == DUMMY_DOUBLE || high[currentSymbolIndex][i] == DUMMY_DOUBLE || low[currentSymbolIndex][i] == DUMMY_DOUBLE || close[currentSymbolIndex][i] == DUMMY_DOUBLE || volume[currentSymbolIndex][i] == DUMMY_DOUBLE){
            fullyFilled = 0;
            break;
        }
    }

    // only record point if using daywise backtesting
    if(daywiseBacktesting){
        recordPointDaywise();
        updateAfterBar();
        return;
    }
    
    // if not using daywise backtesting

    // update the earliest recorded time today
    int minutesSinceOpen = -1;
    if(currentTime >= 930 && currentTime <= 2359) minutesSinceOpen = time_index(currentTime);
    if(currentTime > 2359) minutesSinceOpen = INT_MAX;
    if(earliestTimeDay[currentSymbolIndex][currentDateIndex] == DUMMY_INT){
        // first time today
        earliestTimeDay[currentSymbolIndex][currentDateIndex] = currentTime;
    }


    // when day changes
    if(lastDateTime[currentSymbolIndex] / 10000 != currentDateTime / 10000 && lastDateTime[currentSymbolIndex] != DUMMY_INT){
        
        // if first close filled, then fully fill ohlcv, simulate the trades on this day, and clear ohlcv
        if(close[currentSymbolIndex][0] != DUMMY_DOUBLE){
            simulateTradesMinutewise(currentSymbolIndex, date_index.at(lastDateTime[currentSymbolIndex] / 10000));
        }
    }
    
    // bars before open
    // PASS

    // bars within hours
    if(minutesSinceOpen >= 0 && minutesSinceOpen < numMinutesPerDay){

        if(close[currentSymbolIndex][0] == DUMMY_DOUBLE){
            // on the first bar within hours today, fill the minutes before this one with this open (decidedly NOT lastPrice[currentSymbolIndex])
            for(int t=0;t<minutesSinceOpen;t++){
                open[currentSymbolIndex][t] = currentOHLCV[0];
                high[currentSymbolIndex][t] = currentOHLCV[0];
                low[currentSymbolIndex][t] = currentOHLCV[0];
                close[currentSymbolIndex][t] = currentOHLCV[0];
                volume[currentSymbolIndex][t] = 0;
                dailyHigh[currentSymbolIndex][currentDateIndex] = currentOHLCV[0];
                dailyLow[currentSymbolIndex][currentDateIndex] = currentOHLCV[0];
                dailyHighTime[currentSymbolIndex][currentDateIndex] = 930;
                dailyLowTime[currentSymbolIndex][currentDateIndex] = 930;
                dailyChange[currentSymbolIndex][currentDateIndex] = 0.0;
                dailyChangePortion[currentSymbolIndex][currentDateIndex] = 0.0;
            }
        }

        // fill this minute with the current values
        open[currentSymbolIndex][minutesSinceOpen] = currentOHLCV[0];
        high[currentSymbolIndex][minutesSinceOpen] = currentOHLCV[1];
        low[currentSymbolIndex][minutesSinceOpen] = currentOHLCV[2];
        close[currentSymbolIndex][minutesSinceOpen] = currentOHLCV[3];
        volume[currentSymbolIndex][minutesSinceOpen] = currentOHLCV[4];
        int lastMinuteRecorded = minutesSinceOpen - 1;
        while(lastMinuteRecorded >= 0){
            if(close[currentSymbolIndex][lastMinuteRecorded] != DUMMY_DOUBLE){
                // extend last recorded bar up to this one
                for(int t=lastMinuteRecorded+1;t<minutesSinceOpen;t++){
                    open[currentSymbolIndex][t] = close[currentSymbolIndex][t-1];
                    high[currentSymbolIndex][t] = close[currentSymbolIndex][t-1];
                    low[currentSymbolIndex][t] = close[currentSymbolIndex][t-1];
                    close[currentSymbolIndex][t] = close[currentSymbolIndex][t-1];
                    volume[currentSymbolIndex][t] = 0;
                }
                break;
            }
            lastMinuteRecorded--;
        }
    }

    // bars after close
    // PASS

    updateAfterBar();
}

// read one file
void readFile(bool daywiseBacktesting){
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
        readLine(numValuesPerLine);
    }

    f.close();
}

// read every file in the folder given by this address
void readAllFiles(bool daywiseBacktesting, string path) {

    checkColumnCodes();

    checkStoredTimes();

    cout << "Reading files in folder " << path << "... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    numFiles = 0;
    numFilesComplete = 0;
    bool readCustomFiles = numFiles > 0;
    if(readCustomFiles) numFiles = size(filesToRead);
    int numFilesTotal = 0;
    
    // get the number of files
    for (const auto& entry : fs::directory_iterator(path)){
        if(!readCustomFiles){
            numFiles++;
        }
        numFilesTotal++;
    }

    // read all files
    for (const auto& entry : fs::directory_iterator(path)){
        // print the loading bar or number of files complete depending on settings
        if(btPrintLoading){
            cout << "[";
            for(int i=1;i<=btLoadingBarWidth;i++){
                double pct = (double)numFilesComplete / (double)numFiles;
                if(pct >= (double)i / (double)btLoadingBarWidth){
                    cout << "=";
                }else{
                    cout << " ";
                }
            }
            double pct = (double)numFilesComplete / (double)numFiles;
            cout << "] " << (int)(pct * 10000.0) / 100 << "." << ((int)(pct * 10000.0) / 10) % 10 << (int)(pct * 10000.0) % 10 << "% (" << numFilesComplete << "/" << numFiles << ")\r";
            cout.flush();
        }else{
            if(numFilesComplete % btPrintLoadingInterval == 0){
                cout << numFilesComplete << "/" << numFiles << " files complete.\n";
            }
        }

        fileAddress = entry.path().string();
        if(readCustomFiles){
            for(int i=0;i<numFiles;i++){
                if(filesToRead[i] == fileAddress){
                    readFile(daywiseBacktesting);
                    break;
                }
            }
        }else{
            readFile(daywiseBacktesting);
        }
        numFilesComplete++;
    }

    if(!daywiseBacktesting){
        // for every symbol filled with data at the end, simulate trades on this last day
        for(int i=0;i<maxNumSymbols;i++){
            if(close[i][0] != DUMMY_DOUBLE){
                int d = date_index.at(lastDateTime[i] / 10000);
                simulateTradesMinutewise(i, d);
            }
        }
    }

    if(btPrintLoading){
        cout << "[";
        for(int i=1;i<=btLoadingBarWidth;i++){
            cout << "=";
        }
        cout << "] 100.00% (" << numFiles << "/" << numFiles << ")\n";
    }

    cout << "Finished reading " << numFiles << " of " << numFilesTotal << " files in this folder.\n\n";
}

// setup the data storage
void setup() {

    cout << "Setting up the program... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    int i = 0, j = 0;

    numDates = 0;
    numSymbols = 0;
    numAllowedTickers = 0;
    
    p = (double***)malloc(sizeof(double**) * maxNumDates);

    for(int i=0;i<maxNumDates;i++){
        double** q = (double**)malloc(sizeof(double*) * maxNumSymbols);
        for(int j=0;j<maxNumSymbols;j++){
            double* r = (double*)malloc(sizeof(double) * numStoredTimes);
            for(int k=0;k<numStoredTimes;k++){
                r[k] = DUMMY_DOUBLE;
            }
            q[j] = r;
        }
        p[i] = q;
        numBarsByDateIndex[i] = 0;
        numFilledByDateIndex[i] = 0;
    }

    for (i = 0; i < maxNumSymbols; i++) {
        numBars[i] = 0;
        lastDateTime[i] = DUMMY_INT;
        lastPrice[i] = DUMMY_DOUBLE;
        currentDailyRangeIndex[i] = DUMMY_DOUBLE;

        for (j = 0; j < maxNumDates; j++) {
            totalVolume[i][j] = 0;

            earliestTimeDay[i][j] = DUMMY_INT;

            dailyOpen[i][j] = DUMMY_DOUBLE;
            dailyClose[i][j] = DUMMY_DOUBLE;
            dailyHigh[i][j] = DUMMY_DOUBLE;
            dailyLow[i][j] = DUMMY_DOUBLE;
            dailyRange[i][j] = DUMMY_DOUBLE;

            dailyHighTime[i][j] = DUMMY_INT;
            dailyLowTime[i][j] = DUMMY_INT;

            dailyChange[i][j] = DUMMY_DOUBLE;
            dailyGap[i][j] = DUMMY_DOUBLE;
            dailyChangePortion[i][j] = DUMMY_DOUBLE;
            dailyGapPortion[i][j] = DUMMY_DOUBLE;
        }

        for (j = 0; j < maxMALength; j++) {
            ma[i][j] = DUMMY_DOUBLE;
        }

        userTickers[i].clear();
        userVolumes[i] = DUMMY_INT;
        userMarketCaps[i] = DUMMY_INT;

        for (j = 0; j < mwbtNumTrialsTotal; j++) {
            entryPrices[i][j] = DUMMY_DOUBLE;
            entryMinutesSinceOpen[i][j] = DUMMY_INT;
        }

        for(j = 0; j < numMinutesPerDay; j++){
            
            open[i][j] = DUMMY_DOUBLE;
            high[i][j] = DUMMY_DOUBLE;
            low[i][j] = DUMMY_DOUBLE;
            close[i][j] = DUMMY_DOUBLE;
            volume[i][j] = DUMMY_DOUBLE;
        }
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

            double* P = p[i][j];
            
            for (k = 0; k < numStoredTimes; k++) {
                if (P[k] == DUMMY_DOUBLE) {
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
                if (P[k] < 0.0) { // in case it's somehow negative rather than DUMMY_DOUBLE (should be impossible)
                    cerr << "Exits[" << i << "][" << numFilledByDateIndex[i] << "] was empty. This must have been because a data value from symbol " << j << " on date " << index_date.at(i) << " was negative.\n\n";
                    exit(1);
                }
            }
                
            // calculation of entries and exits (depends on strategy)
            if(P[0] == 0.0){
                entriesDay[numFilledByDateIndex[i]] = 0.0;
            }else{
                entriesDay[numFilledByDateIndex[i]] = (P[1] - P[0]) / P[0];
            }
                
            if(P[1] == 0.0){
                exitsDay[numFilledByDateIndex[i]] = 0.0;
            }else{
                exitsDay[numFilledByDateIndex[i]] = ((P[2] - P[1]) / P[1]) * btLeverage;
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

void analyzeDaywise() {
    cout << "Backtesting with " << numAllowedTickers << " symbols allowed to trade... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";

    if(numDates < 1){
        cerr << "The number of dates found in the dataset was " << numDates << ". It must be at least 1 to backtest.\n\n";
        exit(1);
    }
    if(numSymbols < 1){
        cerr << "The number of ticker symbols found in the dataset was " << numSymbols << ". It must be at least 1 to backtest.\n\n";
        exit(1);
    }
    
    if (btStartingDateIndex < 0) btStartingDateIndex = numDates + btStartingDateIndex;
    if (btStartingDateIndex < 0) btStartingDateIndex = 0;
    if (btStartingDateIndex >= numDates) btStartingDateIndex = numDates - 1;

    if (btEndingDateIndex < 0) btEndingDateIndex = numDates + btEndingDateIndex;
    if (btEndingDateIndex < 0) btEndingDateIndex = 0;
    if (btEndingDateIndex >= numDates) btEndingDateIndex = numDates - 1;

    int tradeSymbolIndexByDate = 0;
    long long previousVolume = 0;

    string tradeSymbol = "";

    for (int dateIndex = btStartingDateIndex; dateIndex <= btEndingDateIndex; dateIndex++) {
        tradeSymbolIndexByDate = -1;

        for (int i = 0; i < dwbtNumSymbolsPerDay; i++) {
            
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
                for (int j = 0; j < btPreviousVolumeLookBackLength; j++) {
                    if (dateIndex - j - 1 >= 0) {
                        previousVolume += totalVolume[symbol_index.at(tradeSymbol)][dateIndex - j - 1];
                    }
                    else {
                        // stop if reaching day 0
                        break;
                    }
                }

                if (previousVolume < dwbtMinPreviousVolume || previousVolume > dwbtMaxPreviousVolume) {
                    continue;
                }

                if (btDisregardFilters) {
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

            // trade this symbol (in all experiments where the # stocks traded each day >= this stock's index on this day)
            btResult = exits[dateIndex][tradeSymbolIndexByDate];
            if(btPrintAllResults && !btPrintDetailedResults && !btPrintLoading){
                cout << btResult << " ";
            }
            if(btPrintDetailedResults && !btPrintLoading){
                cout << "Exit: " << exits[dateIndex][tradeSymbolIndexByDate] << "\nSymbol: " << tradeSymbol << "\nDate: " << index_date.at(dateIndex) << "\n";
            }
            if(btPrintEntries && !btPrintLoading){
                cout << "Entry: " << entries[dateIndex][tradeSymbolIndexByDate] << "\n";
            }
            if(btResult <= btMinOutlier || btResult >= btMaxOutlier){
                dwbtNumOutliers[i]++;
                if(btPrintOutliers && !btPrintLoading){
                    cout << "Outlier: " << btResult << " " << tradeSymbol << " " << index_date.at(dateIndex) << "\n";
                }
                if(btIgnoreOutliers){
                    btResult = 0.0;
                }
            }
            if (btResult > 0.0) {
                for(int j=i;j<dwbtNumSymbolsPerDay;j++){
                    dwbtWins[j]++;
                    dwbtWon[j] += btResult;
                }
            }
            else {
                if (btResult < 0.0) {
                    for(int j=i;j<dwbtNumSymbolsPerDay;j++){
                        dwbtLosses[j]++;
                        dwbtLost[j] += btResult;
                    }
                }
                else {
                    for(int j=i;j<dwbtNumSymbolsPerDay;j++){
                        dwbtTies[j]++;
                    }
                }
            }

            // update account balance stats
            for(int j=i;j<dwbtNumSymbolsPerDay;j++){
                dwbtVar[j] += btResult * btResult;
                dwbtBalance[j] *= (1.0 + btResult);
            }
        }
    }
    
    string output = "";

    output += "Backtested daywise from date with index " + to_string(btStartingDateIndex) + " to index " + to_string(btEndingDateIndex) + ", " + to_string(btEndingDateIndex - btStartingDateIndex + 1) + " days.\n\n";
    
    output += "SUMMARY:\n\nTrades: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtWins[j] + dwbtLosses[j] + dwbtTies[j]) + " ";
    }
    
    output += "\nWins: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtWins[j]) + " ";
    }
    
    output += "\nLosses: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtLosses[j]) + " ";
    }
    
    output += "\nTies: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtTies[j]) + " ";
    }

    output += "\nPercentage Won: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtWon[j] * 100.0) + "   ";
    }

    output += "\nPercentage Lost: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtLost[j] * 100.0) + " ";
    }

    output += "\nTotal Percentage Gained: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string((dwbtWon[j] + dwbtLost[j]) * 100.0) + " ";
    }

    output += "\nProfit Factor: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(-1.0 * dwbtWon[j] / dwbtLost[j]) + " ";
    }

    output += "\nFinal Balance: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtBalance[j]) + " ";
    }

    output += "\nVariance of Trade Results From Zero: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        dwbtVar[j] /= (double)(dwbtWins[j] + dwbtLosses[j] + dwbtTies[j]);
        output += to_string(dwbtVar[j]) + " ";
    }

    output += "\nStandard Deviation of Trade Results From Zero: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        dwbtSD[j] = sqrt(dwbtVar[j]);
        output += to_string(dwbtSD[j]) + " ";
    }

    output += "\nSharpe Ratio: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string((dwbtWon[j] + dwbtLost[j]) / dwbtSD[j]) + " ";
    }

    output += "\nOutliers: ";
    for(int j=0;j<dwbtNumSymbolsPerDay;j++){
        output += to_string(dwbtNumOutliers[j]) + " ";
    }

    aggregateBacktestingResults = output;
    if(btPrintSummary){
        cout << output << "\n";
    }
}

void analyzeMinutewise() {
    
    string output = "";

    output += "Backtested minutewise over " + to_string(numDates) + " days.\n\n";

    for(int i=0;i<mwbtNumTrialsTotal;i++){
    
        output += "SUMMARY " + to_string(i / (mwbtNumTrialsSecond * mwbtNumTrialsThird)) + " " + to_string((i / mwbtNumTrialsThird) % mwbtNumTrialsSecond) + " " + to_string(i % mwbtNumTrialsThird) + ":\n\n";
        output += "Trades/Wins/Losses/Ties: " + to_string(mwbtWins[i] + mwbtLosses[i] + mwbtTies[i]) + "/" + to_string(mwbtWins[i]) + "/" + to_string(mwbtLosses[i]) + "/" + to_string(mwbtTies[i]);

        output += "\nPercentage Won/Lost/Net: " + to_string(mwbtWon[i] * 100.0) + "/" + to_string(mwbtLost[i] * 100.0) + "/" + to_string((mwbtWon[i] + mwbtLost[i]) * 100.0);

        output += "\nProfit Factor: ";
        output += to_string(-1.0 * mwbtWon[i] / mwbtLost[i]);

        output += "\nFinal Balance: ";
        output += to_string(mwbtBalance[i]);

        output += "\nVariance of Trade Results From Zero: ";
        mwbtVar[i] /= (double)(mwbtWins[i] + mwbtLosses[i] + mwbtTies[i]);
        output += to_string(mwbtVar[i]);

        output += "\nStandard Deviation of Trade Results From Zero: ";
        mwbtSD[i] = sqrt(mwbtVar[i]);
        output += to_string(mwbtSD[i]);

        output += "\nSharpe Ratio: ";
        output += to_string((mwbtWon[i] + mwbtLost[i]) / mwbtSD[i]);

        output += "\nThere were " + to_string(mwbtNumOutliers[i]) + " outlying trade results.\n\n";
    }

    aggregateBacktestingResults = output;
    if(btPrintSummary){
        cout << output << "\n";
    }
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

void stringBucketSort(vector<string>& v, int n){
    // find the length of longest string
    int m = size(v[0]);
    for(int i=1;i<n;i++){
        m = max(m, (int)size(v[i]));
    }

    vector<vector<string>> b(256, vector<string>(0, ""));

    for(int i=m-1;i>=0;i--){
        // clear all buckets
        for(int j=0;j<256;j++){
            b[j].clear();
        }

        // put all values in buckets according to the value of the ith character
        for(int j=0;j<n;j++){
            if(size(v[j]) > i){
                b[v[j][i]].push_back(v[j]);
            }else{
                b[0].push_back(v[j]);
            }
        }

        // reconcatenate all buckets
        int spot = 0;
        for(int j=0;j<256;j++){
            for(int k=0;k<size(b[j]);k++){
                v[spot] = b[j][k];
                spot++;
            }
        }
    }
}

void printSymbolsFromFiles(bool alphabetized){
    if(numSymbols == 0){
        cout << "No symbols have been found yet in " << numFilesComplete << " files.\n";
        return;
    }
    cout << "Printing " << numSymbols << " symbols found in " << numFilesComplete << " files.\n";

    if(alphabetized){
        vector<string> v(numSymbols, "");
        for(int i=0;i<numSymbols;i++){
            v[i] = index_symbol.at(i);
        }
        stringBucketSort(v, numSymbols);
        for(int i=0;i<numSymbols;i++){
            cout << v[i] << " ";
        }
    }else{
        for(int i=0;i<numSymbols;i++){
            cout << index_symbol.at(i) << " ";
        }
    }
    cout << "\nPrinted " << numSymbols << " symbols found in " << numFilesComplete << " files.\n\n";
}

void backtestDaywise(string readPath, int numVolumeDays, int minVolume, int maxVolume, bool includePartialDailyVolumeSamples, bool sortSymbolEntriesAscending){

    btResult = 0.0;
    for(int i=0;i<dwbtNumSymbolsPerDay;i++){
        dwbtWins.push_back(0);
        dwbtLosses.push_back(0);
        dwbtTies.push_back(0);
        dwbtWon.push_back(0.0);
        dwbtLost.push_back(0.0);
        dwbtBalance.push_back(1.0);
        dwbtVar.push_back(0.0);
        dwbtSD.push_back(0.0);
        dwbtNumOutliers.push_back(0);
    }

    // filesToRead = {"C:\\Minute Stock Data\\2025-05-12.csv"};
    readAllFiles(true, readPath);
    setupDateData(numVolumeDays, minVolume, maxVolume, includePartialDailyVolumeSamples);
    sortDateData(sortSymbolEntriesAscending);
    analyzeDaywise();
}

void backtestMinutewise(string readPath){

    if(mwbtEarliestTimeToTrade < 930 || mwbtEarliestTimeToTrade > 1559){
        cerr << "mwbtEarliestTimeToTrade must be between 930 and 1559. It is " << mwbtEarliestTimeToTrade << ".\n\n";
        exit(1);
    }
    if(mwbtEarliestTimeToTrade % 100 > 59){
        cerr << "The minute of mwbtEarliestTimeToTrade must be between 0 and 59. It is " << mwbtEarliestTimeToTrade % 100 << ".\n\n";
        exit(1);
    }
    if(mwbtLatestTimeToTrade < 930 || mwbtLatestTimeToTrade > 1559){
        cerr << "mwbtLatestTimeToTrade must be between 930 and 1559. It is " << mwbtLatestTimeToTrade << ".\n\n";
        exit(1);
    }
    if(mwbtLatestTimeToTrade % 100 > 59){
        cerr << "The minute of mwbtLatestTimeToTrade must be between 0 and 59. It is " << mwbtLatestTimeToTrade % 100 << ".\n\n";
        exit(1);
    }

    btResult = 0.0;
    for(int i=0;i<mwbtNumTrialsTotal;i++){
        mwbtWins.push_back(0);
        mwbtLosses.push_back(0);
        mwbtTies.push_back(0);
        mwbtWon.push_back(0.0);
        mwbtLost.push_back(0.0);
        mwbtBalance.push_back(1.0);
        mwbtVar.push_back(0.0);
        mwbtSD.push_back(0.0);
        mwbtNumOutliers.push_back(0);
    }

    // filesToRead = {"C:\\Minute Stock Data\\2025-05-12.csv"};
    readAllFiles(false, readPath);
    analyzeMinutewise();
}

void saveBacktestingResults(string address){
    ofstream f(address);

    if (!f.is_open()) {
        cerr << "File " << address << " was found but could not be opened.\n\n";
        exit(1);
    }
    f << aggregateBacktestingResults;
    f.close();
}

int main() {
    setup();

    string readPath = "C:\\Minute Stock Data\\";
    columnCodes = "SVOCHLU-";

    // set backtesting settings:
    // int btStartingDateIndex, int btEndingDateIndex, int dwbtNumSymbolsPerDay, long long dwbtMinPreviousVolume, long long dwbtMaxPreviousVolume,
    // int btPreviousVolumeLookBackLength, int mwbtEarliestTimeToTrade, int mwbtLatestTimeToTrade, double btLeverage, bool btDisregardFilters, double btMinOutlier, double btMaxOutlier, bool btIgnoreOutliers,
    // bool btPrintOutliers, bool btPrintEntries, bool btPrintAllResults, bool btPrintDetailedResults,
    // bool btPrintLoading, bool btPrintLoadingInterval, bool btPrintSummary
    btStartingDateIndex = 0;
    btEndingDateIndex = -1;
    dwbtNumSymbolsPerDay = 5;
    dwbtMinPreviousVolume = 0;
    dwbtMaxPreviousVolume = INT_MAX;
    btPreviousVolumeLookBackLength = 1;
    mwbtEarliestTimeToTrade = 930;
    mwbtLatestTimeToTrade = 1559;
    btLeverage = 1.0;
    btDisregardFilters = true;
    btMinOutlier = -0.5;
    btMaxOutlier = 1.0;
    btIgnoreOutliers = false;
    btPrintOutliers = true;
    btPrintEntries = true;
    btPrintAllResults = true;
    btPrintDetailedResults = true;
    btPrintLoading = true;
    btPrintLoadingInterval = 5;
    btLoadingBarWidth = 60;
    btPrintSummary = true;

    //backtestDaywise(readPath, 10, 0, INT_MAX, true, true);
    backtestMinutewise(readPath);

    saveBacktestingResults("C:\\Notes\\Accurate Backtesting Results\\Current Aggregate Backtesting Results.txt");

    //vector<string> banned = { "" };
    //extractSymbolData("C:\\Stock Symbols\\symbolDataNASDAQ.txt", "symbol:", "volume:", "marketCap:", 1, 0, -6);
    //filterSymbols(0, LLONG_MAX, 0, LLONG_MAX, banned);
    //printAllowedSymbols(false);

    //printSymbolsFromFiles(true);
    
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
