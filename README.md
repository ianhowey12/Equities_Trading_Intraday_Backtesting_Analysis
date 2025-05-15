# Equities_Trading_Intraday_Backtesting_Analysis
Backtest trading strategies on your own intraday equities data across multiple ticker symbols and dates.

Allows filtering by dates, trading volume, and market cap.

Change the readPath in main() to the path local folder containing your intraday equities data files. These files must be named in ascending lexicographical order as the dates of the data within them ascend. Naming files according to date (YYYY-MM-DD) satisfies this requirement.

Change filesToRead to the paths of the individual files whose data should be used. Or leave it blank to use every file in the specified directory.

Change columnCodes according to the format of each line in every file. This should be a list of capital letters and dashes corresponding to what each comma-separated value represents.

- S = ticker symbol (ABCD)
- T = time (HH:MM or HH:MM:SS)
- D = date (YYYY-MM-DD)
- U = Unix timestamp
- M = moment or datetime (YYYY-MM-DD HH:MM or YYYY-MM-DD HH:MM:SS)
- O = open
- H = high
- L = low
- C = close
- Dash = unused value

You can also input data as a text file and extract filtered ticker symbols along with volume and market cap data. Changing the file address in the call to extractSymbolData() within main() lets you do this. This h3lps with organizing hundreds or thousands of symbols.
