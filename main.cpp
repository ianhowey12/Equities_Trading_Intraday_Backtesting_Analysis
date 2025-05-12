#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <fstream>

using namespace std;

vector<char> symbolData;

int numFiles = 0;

int currentSymbolIndex = 0;
int currentDateIndex = 0;
int numSymbols = 0;

int d[11160]; // 30Y * 12M * 31D
int numDates = 0;
vector<int> D;
vector<vector<char>> t;

// prices and date indices that have not been filled (inside p[] and d[])
#define DUMMY_INT INT_MIN
// empty entries[] and exits[] values (unnecessary)
#define DUMMY_DOUBLE (double)INT_MIN

#define maxNumSymbols 6000
#define maxNumDates 1000
#define numStoredTimes 3
// times whose bars should be stored and used to backtest
vector<int> selectedTimes = { 900, 905, 1500 };
// metrics (OHLCV) of the bars that should be stored and used to backtest
vector<int> selectedMetrics = { 'o', 'c', 'c' };

// data points
int p[maxNumSymbols * maxNumDates * numStoredTimes];

// number of symbols used in each date
int numPointsByDateIndex[maxNumDates];

// values for each day and symbol used to determine trade entry and exit values
int tradeSymbolIndices[maxNumDates][maxNumSymbols];
double entries[maxNumDates][maxNumSymbols];
double exits[maxNumDates][maxNumSymbols];

// values that sweep across all dates, filling in holes in price with last price value recorded for that symbol
//int priceSweeper[maxNumSymbols];

// cumulative volume for each symbol and date
long long totalVolume[maxNumSymbols][maxNumDates];


// predefined user-created symbol information for user symbol organization and volume/market cap filtering during backtesting
vector<char> userTickers[maxNumSymbols];
long long userVolumes[maxNumSymbols];
long long userMarketCaps[maxNumSymbols];

// symbols allowed to trade. filter by volume and market cap using data from the above arrays.
int numAllowedTickers = 0;
vector<char> allowedTickers[maxNumSymbols];
int allowedToUserIndex[maxNumSymbols];


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

vector<char> dateToString(int d) {
    vector<char> s = { '2','0' };
    s.push_back((char)(d / (10 * 12 * 31)) + '0');
    d %= 10 * 12 * 31;
    s.push_back((char)(d / (12 * 31)) + '0');
    d %= 12 * 31;
    s.push_back((char)(d / (10 * 31)) + '0');
    d %= 10 * 31;
    s.push_back((char)(d / (31)) + '0');
    d %= 10;
    s.push_back((char)(d / (10)) + '0');
    s.push_back((char)(d) + '0');
    return s;
}

bool read(string address, int numDummyValuesAfterTime){
    ifstream f(address);

    if (!f.is_open()) {
        return 0;
    }

    string s = "";

    getline(f, s);

    int currentFileBar = -1;
    while (getline(f, s)) {
        currentFileBar++;
        int n = s.length();

        if (n < 20) {
            cerr << "Bar " << currentFileBar << " in file " << address << " with a length of " << n << "characters was incorrectly shorter than 20 characters long. Make sure it includes a date and time, OHLCV values, and a ticker symbol.\n\n";
            exit(1);
        }

        if (!(isNumeric(s[0]) && isNumeric(s[1]) && isNumeric(s[2]) && isNumeric(s[3]) && isNumeric(s[5]) && isNumeric(s[6]) && isNumeric(s[8]) && isNumeric(s[9]) && isNumeric(s[11]) && isNumeric(s[12]) && isNumeric(s[14]) && isNumeric(s[15]) && s[4] == '-' && s[7] == '-')) {
            cerr << "Bar " << currentFileBar << " in file " << address << " does not have the correct date and time format (YYYY-MM-DD HH:MM:SS).\n\n";
            exit(1);
        }

        // determine if time is one of the selected times
        int time = (s[11] - '0') * 1000 + (s[12] - '0') * 100 + (s[14] - '0') * 10 + (s[15] - '0');
        int selectedTimeIndex = -1;
        for (int i = 0; i < numStoredTimes; i++) {
            if (time == selectedTimes[i]) {
                selectedTimeIndex = i;
                break;
            }
        }
        // if not, move on to the next bar
        if (selectedTimeIndex == -1) {
            continue;
        }

        // date
        int date = ((s[2] - '0') * 10 + s[3] - '0') * 12 * 31 + ((s[5] - '0') * 10 + s[6] - '0') * 31 + ((s[8] - '0') * 10 + s[9] - '0');
        if (date < 0) {
            cerr << "Date at beginning of line corresponding to bar " << currentFileBar << " in file " << address << " was before 2000/01/01.\n\n";
            exit(1);
        }
        if (date >= 11160) {
            cerr << "Date at beginning of line corresponding to bar " << currentFileBar << " in file " << address << " was past 2030/01/01.\n\n";
            exit(1);
        }

        int dateIndex = -1;

        // plug this date into date to index to see if it has been recorded yet
        if (d[date] != DUMMY_INT) {
            dateIndex = d[date];
        }
        else {
            d[date] = numDates;
            D.push_back(date);
            dateIndex = numDates;
            numDates++;
        }

        // handle dates that are out of order
        if (dateIndex > 0) {
            if (D[dateIndex - 1] > D[dateIndex]) {
                string dateString1 = "";
                vector<char> dateVector1 = dateToString(d[dateIndex - 1]);
                for (int i = 0; i < size(dateVector1); i++) {
                    dateString1 += dateVector1[i];
                }
                string dateString2 = "";
                vector<char> dateVector2 = dateToString(d[dateIndex]);
                for (int i = 0; i < size(dateVector2); i++) {
                    dateString2 += dateVector2[i];
                }
                cerr << "Consecutively parsed dates " << dateString1 << " and " << dateString2 << " were not entered in ascending order.\n\n";
                exit(1);
            }
        }

        // moving to dummy values
        int spot = 16;
        while (s[spot] != ',') {
            spot++;
            if (spot >= n) {
                cerr << "Bar " << currentFileBar << " in file " << address << " ended without a comma after the time " << time << " .\n\n";
                exit(1);
            }
        }

        // moving past dummy values
        for (int i = 0; i < numDummyValuesAfterTime; i++) {
            spot++;
            while (s[spot] != ',') {
                spot++;
                if (spot >= n) {
                    cerr << "Bar " << currentFileBar << " in file " << address << " ended without a comma after dummy value " << i << " .\n\n";
                    exit(1);
                }
            }
        }

        // ticker symbol
        vector<char> ticker;
        int tickerSpot = n - 2;
        while (s[tickerSpot] != ',') {
            tickerSpot--;
        }
        for (int i = tickerSpot + 1; i < n; i++) {
            ticker.push_back(s[i]);
        }
        bool tickerFound = 0;
        int tickerIndex = -1;
        for (int i = 0; i < size(t); i++) {
            if (t[i] == ticker) {
                tickerFound = 1;
                tickerIndex = i;
                break;
            }
        }
        if (!tickerFound) {
            tickerIndex = size(t);
            t.push_back(ticker);
        }

        // OHLCV values
        for (int i = 0; i < 5; i++) {
            spot++;
            int subSpot = spot + 1;
            while (s[subSpot] != ',' && s[subSpot] != '.') {
                subSpot++;
                if (subSpot >= n) {
                    cerr << "Bar " << currentFileBar << " in file " << address << " ended without a comma after OHLCV value " << i << " .\n\n";
                    exit(1);
                }
            }
            int num = 0;
            // get digits before .
            for (int j = spot; j < subSpot; j++) {
                if (i == 4) { // volume stores value
                    num += (s[j] - '0') * (int)power(10, subSpot - j - 1);
                }
                else { // others store 100 * value
                    num += (s[j] - '0') * (int)power(10, subSpot - j + 1);
                }
            }
            // get two digits after .
            if (s[subSpot] == '.') {
                subSpot += 2;
                if (subSpot >= n) {
                    cerr << "Bar " << currentFileBar << " in file " << address << " ended without two characters after the decimal point of OHLCV value " << i << " .\n\n";
                    exit(1);
                }
                if (s[subSpot - 1] != ',') {
                    num += (s[subSpot - 1] - '0') * 10;
                    if (s[subSpot] != ',') {
                        num += (s[subSpot] - '0');
                    }
                }
            }

            // handle array overflows
            if (tickerIndex >= maxNumSymbols) {
                cerr << "Bar " << currentFileBar << " in file " << address << " overflowed the maximum number of ticker symbols allocated: " << maxNumSymbols << " .\n\n";
                exit(1);
            }
            if (dateIndex >= maxNumDates) {
                cerr << "Bar " << currentFileBar << " in file " << address << " overflowed the maximum number of dates allocated: " << maxNumDates << " .\n\n";
                exit(1);
            }
            if (selectedTimeIndex >= numStoredTimes) {
                cerr << "Bar " << currentFileBar << " in file " << address << " overflowed the maximum number of stored times per day allocated: " << numStoredTimes << " .\n\n";
                exit(1);
            }

            // add this volume (if currently reading volume) to cumulative volume for this symbol and date
            if (i == 4) {
                totalVolume[tickerIndex][dateIndex] += num;
            }

            // add data to OHLCV arrays
            int a = selectedTimeIndex + (dateIndex + tickerIndex * maxNumDates) * numStoredTimes;
            switch (i) {
            case 0:
                if (selectedMetrics[selectedTimeIndex] == 'o') p[a] = num;
                break;
            case 1:
                if (selectedMetrics[selectedTimeIndex] == 'h') p[a] = num;
                break;
            case 2:
                if (selectedMetrics[selectedTimeIndex] == 'l') p[a] = num;
                break;
            case 3:
                if (selectedMetrics[selectedTimeIndex] == 'c') p[a] = num;
                //priceSweeper[tickerIndex] = num;    // update the price sweeper's value for this symbol to fill in future holes
                break;
            case 4:
                if (selectedMetrics[selectedTimeIndex] == 'v') p[a] = num;
                break;
            }

            // move to next comma in case we're in the middle of many decimal places
            while (s[subSpot] != ',') {
                subSpot++;
                if (subSpot >= n) {
                    cerr << "Bar " << currentFileBar << " in file " << address << " ended without a comma after OHLCV value " << i << " .\n\n";
                    exit(1);
                }
            }
            spot = subSpot;
        }
    }

    f.close();
    return 1;
}

// read every file in the folder given by this address
void readAll(string path) {
    cout << "Reading files in folder " << path << "... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    string a = path + "\\0.csv";
    numFiles = 0;
    while (read(a, 3)) {
        numFiles++;
        a = path + "\\" + to_string(numFiles) + ".csv";
    }
    cout << "Finished reading " << numFiles << " files.\n\n";
}

// setup the data storage
void setup() {
    cout << "Setting up the program... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    int i = 0, j = 0;

    for (i = 0; i < maxNumSymbols * maxNumDates * numStoredTimes; i++) {
        p[i] = DUMMY_INT;
    }

    for (i = 0; i < 11160; i++) {
        d[i] = DUMMY_INT;
    }

    numAllowedTickers = 0;

    for (i = 0; i < maxNumSymbols; i++) {
        //priceSweeper[i] = 0;
        for (j = 0; j < maxNumDates; j++) {
            tradeSymbolIndices[j][i] = DUMMY_INT;
            entries[j][i] = DUMMY_DOUBLE;
            exits[j][i] = DUMMY_DOUBLE;

            totalVolume[i][j] = 0;
        }

        userTickers[i].clear();
        userVolumes[i] = DUMMY_INT;
        userMarketCaps[i] = DUMMY_INT;
        allowedTickers[i].clear();
        allowedToUserIndex[i] = 0;
    }
}

void setupDateData() {
    cout << "Setting up the date data storage... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    int i = 0, j = 0, k = 0, l = 0;

    // categorize all data points from p[] by date (group into dates)
    for (i = 0; i < maxNumDates; i++) {
        numPointsByDateIndex[i] = 0;

        for (j = 0; j < maxNumSymbols; j++) {

            // if all points within this symbol and date are in the dataset (!= DUMMY_INT), store the entry and exit for this symbol and day within a temp array and increment # symbols on this day
            bool allPointsTodayFound = 1;
            // location of the point of this date and symbol (first time) within p[]
            int index = (i + j * maxNumDates) * numStoredTimes;
            for (k = 0; k < numStoredTimes; k++) {
                if (p[index + k] == DUMMY_INT) {
                    allPointsTodayFound = 0;
                }
            }
            if (allPointsTodayFound) {
                // get the index of the symbol traded at this date and symbol
                tradeSymbolIndices[i][numPointsByDateIndex[i]] = j;
                // calculation of entries and exits (depends on strategy)
                entries[i][numPointsByDateIndex[i]] = (100.0 * (double)(p[index + 1] - p[index + 0]) / (double)p[index + 0]);
                exits[i][numPointsByDateIndex[i]] = (100.0 * (double)(p[index + 2] - p[index + 1]) / (double)p[index + 1]);
                // increment number of symbols on this day
                numPointsByDateIndex[i]++;
            }
        }
    }

    // sort the data within every day of points[]
    double temp = 0.0;
    int tempi = 0;
    int numBuckets = 10000;
    vector<vector<double>> buckets(numBuckets, vector<double>(0, 0.0));
    vector<vector<double>> bucketsExits(numBuckets, vector<double>(0, 0.0));
    vector<vector<int>> bucketsIndices(numBuckets, vector<int>(0, 0));
    double maxVal = (double)INT_MIN;
    double minVal = (double)INT_MAX;
    for (i = 0; i < maxNumDates; i++) {

        int numPoints = numPointsByDateIndex[i];
        if (numPoints == 0) continue;
        // get the range of values to initialize the buckets
        for (j = 0; j < numPoints; j++) {
            if (entries[i][j] < minVal) minVal = entries[i][j];
            if (entries[i][j] > maxVal) maxVal = entries[i][j] * 1.0001;
        }
        // clear all buckets from previous use
        for (j = 0; j < numBuckets; j++) {
            buckets[j].clear();
        }
        // fill the buckets with the values to be sorted
        for (j = 0; j < numPoints; j++) {
            int index = (int)((double)numBuckets * (entries[i][j] - minVal) / (maxVal - minVal));
            buckets[index].push_back(entries[i][j]);
            bucketsExits[index].push_back(exits[i][j]);
            bucketsIndices[index].push_back(tradeSymbolIndices[i][j]);
        }
        // iterate over the buckets
        for (j = 0; j < numBuckets; j++) {
            // sort them using exchange sort
            for (k = 0; k < size(buckets[j]); k++) {
                for (l = k + 1; l < size(buckets[j]); l++) {
                    if (buckets[j][k] < buckets[j][l]) {
                        temp = buckets[j][k];
                        buckets[j][k] = buckets[j][l];
                        buckets[j][l] = temp;

                        temp = bucketsExits[j][k];
                        bucketsExits[j][k] = bucketsExits[j][l];
                        bucketsExits[j][l] = temp;

                        tempi = bucketsIndices[j][k];
                        bucketsIndices[j][k] = bucketsIndices[j][l];
                        bucketsIndices[j][l] = tempi;
                    }
                }
            }
        }

        // concatenate the buckets for all data types
        int index = 0;
        for (j = 0; j < numBuckets; j++) {
            for (k = 0; k < size(buckets[j]); k++) {
                entries[i][index] = buckets[j][k];
                exits[i][index] = bucketsExits[j][k];
                tradeSymbolIndices[i][index] = bucketsIndices[j][k];
                index++;
            }
        }
        if (index != numPoints) {
            cerr << "Some data was unexpectedly lost while sorting values within a given date. Number of bucket values was " << index << " and number of points for this date was " << numPoints << ".\n\n";
            exit(1);
        }
    }
}

int dateToIndex(int date) {
    return (((date / 10000) % 100) * 12 + ((date / 100) % 100)) * 31 + (date % 100);
}

void backtest(int startingDateIndex, int endingDateIndex, int numSymbolsPerDay, long long minPreviousVolume, long long maxPreviousVolume, int previousVolumeLookBackLength, double leverage, bool disregardFilters) {
    cout << "Backtesting with " << numAllowedTickers << " symbols allowed to trade... (This may take a few seconds to minutes depending on the amount of data present.)\n\n";
    /*
    if (startingDate < 20000000 || startingDate > 20291231) {
        cerr << "Starting date " << startingDate << " must be between 20000000 and 20291231.\n\n";
        exit(1);
    }
    if (endingDate < 20000000 || endingDate > 20291231) {
        cerr << "Ending date " << endingDate << " must be between 20000000 and 20291231.\n\n";
        exit(1);
    }
    
    int sd = d[dateToIndex(startingDate)];
    if (sd == DUMMY_INT) {
        cerr << "Starting date " << startingDate << " is on a weekend or another day without recorded data.\n\n";
        exit(1);
    }
    int ed = d[dateToIndex(endingDate)];
    if (ed == DUMMY_INT) {
        cerr << "Ending date " << endingDate << " is on a weekend or another day without recorded data.\n\n";
        exit(1);
    }
    */
    
    if (startingDateIndex < 0) startingDateIndex = numDates + startingDateIndex;
    if (startingDateIndex < 0) startingDateIndex = 0;
    if (startingDateIndex >= numDates) startingDateIndex = numDates - 1;

    if (endingDateIndex < 0) endingDateIndex = numDates + endingDateIndex;
    if (endingDateIndex < 0) endingDateIndex = 0;
    if (endingDateIndex >= numDates) endingDateIndex = numDates - 1;

    int wins = 0, losses = 0, ties = 0, trades = 0;
    double won = 0.0, lost = 0.0, total = 0.0;
    double balance = 1.0;

    int tradingSymbol = 0;
    long long previousVolume = 0;

    for (int dateIndex = startingDateIndex; dateIndex <= endingDateIndex; dateIndex++) {

        // if (numSymbolsPerDay > numPointsByDateIndex[dateIndex]) continue;

        tradingSymbol = -1;

        for (int i = 0; i < numSymbolsPerDay; i++) {
            
            // find the next symbol that is allowed, quit for this day if all symbols have been checked
            bool flag = 1;
            while (flag) {
                tradingSymbol++;
                if (tradingSymbol >= numPointsByDateIndex[dateIndex]) break;

                previousVolume = 0;
                int tradeSymbolIndex = tradeSymbolIndices[dateIndex][tradingSymbol];

                // add up all previous day volumes if previous days existed
                for (int j = 0; j < previousVolumeLookBackLength; j++) {
                    if (dateIndex - j - 1 >= 0) {
                        previousVolume += totalVolume[tradeSymbolIndex][dateIndex - j - 1];
                    }
                    else {
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
                    if (tradeSymbolIndex < 0 || tradeSymbolIndex >= size(t)) {
                        cerr << "Index of symbol to trade (" << tradeSymbolIndex << ") is out of the range of the number of all tickers in the dataset.\n\n";
                        exit(1);
                    }
                    for (int j = 0; j < numAllowedTickers; j++) {
                        if (t[tradeSymbolIndex] == allowedTickers[j]) {
                            flag = 0;
                            break;
                        }
                    }
                }
            }

            if (tradingSymbol >= numPointsByDateIndex[dateIndex]) break;;

            // trade this symbol
            double result = exits[dateIndex][tradingSymbol];
            if (result > 0.0) {
                wins++;
                won += result;
            }
            else {
                if (result < 0.0) {
                    losses++;
                    lost -= result;
                }
                else {
                    ties++;
                }
            }

            // update account balance stats
            total += result;
            trades++;
            balance *= (100.0 + result) / 100.0;
        }
    }
    
    cout << "ANALYSIS:\n\nTrades: " << trades << "\nWins: " << wins << "\nLosses: " << losses << "\nTies: " << ties;
    cout << "\nTotal Percentage Gained: " << total << "%\nPercentage Won: " << won << "%\nPercentage Lost: " << lost << "%\nProfit Factor: " << won / lost << "\nFinal Balance: " << balance << "\n\n\n";

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
vector<char> extractTicker(vector<char> s, int i, int spot) {
    int l = size(s);
    vector<char> o;
    while (spot < l) {
        if ((s[spot] < 'a' || s[spot] > 'z') && (s[spot] < 'A' || s[spot] > 'Z') && s[spot] != '.') {
            return o;
        }
        o.push_back(s[spot]);
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
            allowedTickers[numAllowedTickers] = userTickers[i];
            allowedToUserIndex[numAllowedTickers] = i;
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
        if (size(allowedTickers[i]) > 0) {
            for (int j = 0; j < size(allowedTickers[i]); j++) {
                cout << allowedTickers[i][j];
            }
            if (printVolumeAndMarketCap) {
                cout << " " << userVolumes[allowedToUserIndex[i]] << " " << userMarketCaps[allowedToUserIndex[i]] << "\n";
            }
            else {
                cout << " ";
            }
        }
    }
    cout << "\nPrinted " << numAllowedTickers << " allowed symbols.\n\n";
}

int main() {
    setup();

    readAll("C:\\Minute Stock Data\\");

    setupDateData();

    vector<string> banned = { "" };
    extractSymbolData("C:\\Minute Stock Data\\symbolDataNASDAQ.txt", "symbol:", "volume:", "marketCap:", 1, 0, -6);
    filterSymbols(0, LLONG_MAX, 0, LLONG_MAX, banned);
    //printAllowedSymbols(false);
    ////    Comment out either the above line or the line below this to use
    backtest(0, -1, 1, 0, INT_MAX, 1, 0.1, true);
    
    return 0;
}

/*

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

*/
