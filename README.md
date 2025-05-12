# Equities_Trading_Intraday_Backtesting_Analysis
Backtest trading strategies on your own intraday equities data across multiple ticker symbols and dates.

Allows filtering by dates, trading volume, and market cap.

Change the folder path in the call to readAll() within main() to the local folder containing your intraday equities data files. These files must be named 0.csv, 1.csv, 2.csv, and so on.

You can also input data as a text file and extract filtered ticker symbols along with volume and market cap data. Changing the file address in the call to extractSymbolData() within main() lets you do this. This makes organizing hundreds or thousands of symbols easy.


Data files should be formatted to look like the following text:

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

...
