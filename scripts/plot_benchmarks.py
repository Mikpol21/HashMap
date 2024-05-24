import pandas as pd
import sys
import matplotlib.pyplot as plt

if len(sys.argv) < 3:
    print("Usage: python script_name.py <filename>")
    sys.exit(1)

filename = sys.argv[1]
outfile = sys.argv[2]

# Read the CSV file into a pandas DataFrame
df = pd.read_csv(filename)

# Filter out rows with empty 'label' column
df = df.dropna(subset=['name'])

# Split the 'label' column into separate columns for Map, Size, and HitRate
df[['Map', 'Size', 'HitRate']] = df['name'].str.extract(r'BM_find<(\w+), (\d+), HitRate::(\w+)>')



# Convert 'Size' column to integer
df['Size'] = df['Size'].str.extract(r'(\d+)').astype(int)

# Create a separate plot for each HitRate
findspecs = df['HitRate'].unique()

fig, axs = plt.subplots(len(findspecs), 1, figsize=(10, 5*len(findspecs)), dpi=300)

for i, findspec in enumerate(findspecs):
    # Filter DataFrame for the current FindSpec
    dfspec = df[df['HitRate'] == findspec]
    
    # Plot for each Map
    maps = dfspec['Map'].unique()
    for m in maps:
        dfmap = dfspec[dfspec['Map'] == m]

        axs[i].plot(dfmap['Size'], dfmap['cpu_time'], label=m)

    # Add labels and title
    axs[i].set_xlabel('Size')
    axs[i].set_ylabel('CPU Time (ns)')
    axs[i].set_title('HitRate: {}'.format(findspec))
    axs[i].legend()
    axs[i].grid(True)
    axs[i].set_xscale('log')

plt.savefig(outfile)
plt.close()