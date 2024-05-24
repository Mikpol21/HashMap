sudo cpupower frequency-set --governor performance

file_name=results-$(date +"%H:%M-%d-%m-%Y")
./benchmark --benchmark_out_format=csv --benchmark_out=results/${file_name}.csv
tail -n +10 results/${file_name}.csv > ${file_name}_cropped.csv
python3 scripts/plot_benchmarks.py ${file_name}_cropped.csv plots/${file_name}.png
rm ${file_name}_cropped.csv

sudo cpupower frequency-set --governor powersave