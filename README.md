# Trading_Intraday_Backtesting_Analysis
Backtest trading strategies on your own intraday market price data across multiple ticker symbols and dates.

Allows filtering by dates, trading volume, and market cap.

Change the readPath in main() to the path local folder containing your intraday price data files. These files must be named in ascending lexicographical order as the dates of the data within them ascend. Naming files according to date (YYYY-MM-DD) satisfies this requirement. Also, the date-time combinations for every symbol across every file must ascend along with the order of the files.

Change filesToRead to the paths of the individual files whose data should be used. Or leave it blank to use every file in the specified directory.

Change storedTimes and storedMetrics to the data that gets extracted from each symbol on each date. storedTimes should contain the times of the bars being recorded, while storedMetrics should contain the characters ('O'/'H'/'L'/'C'/'V') corresponding to the values extracted from the bars at each of these times. The lengths of these vectors must equal numStoredTimes.

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

You can also input data as a text file and extract filtered ticker symbols along with volume and market cap data. Changing the file address in the call to extractSymbolData() within main() lets you do this. This helps with organizing hundreds or thousands of symbols.

The program has two modes: daywise and minutewise backtesting.
Daywise enters and exits trades at specified times of the day (according to storedTimes and storedMetrics) on a predefined number of assets with the greatest change (by default, but this can be changed) between the first two storedTimes.
Minutewise enters and exits trades based on specified entry and exit conditions at any time of day.

Things to watch out for when backtesting:
- If using a new dataset, test each function individually to make sure the full backtesting process works correctly.
- This backtesting process can cause the following biases:
- Minutewise: Trades that are entered but not exited (because that symbol's data ends before an exit signal is given) are not counted in the results. So if unexited trades are, on average, less profitable due to your strategy, the results will appear more profitable then they should.
- Minutewise: You can be in several trades simultaneously, when in real life, this may not be the case. If many symbols' price movements are correlated, effects of correlated price movements may be overrepresented compared to trading one symbol at a time.
- Minutewise: Entries and exits can only occur on bars that are actually in the dataset. Symbols with lots of dataset holes may be underrepresented compared to a real strategy that enters and exits at any time.
- Daywise: Symbols that start after the first storedTime (only on first day) or end before the final storedTime (only on last day) are not traded and may be underrepresented compared to a real strategy that enters and exits at any time.
