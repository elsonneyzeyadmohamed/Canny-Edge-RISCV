import matplotlib
matplotlib.use('Agg')   # use non-interactive backend — no display needed, saves to file
import matplotlib.pyplot as plt

# Stage names displayed on both charts
stages = ['Gaussian\n5x5', 'Sobel\nGx/Gy', 'Magnitude\nL2', 'Direction',
          'NMS', 'Double\nThreshold', 'Hysteresis']

# Average execution time (ms) per stage measured under -O3 over 100 iterations
times  = [8.97267, 2.77247, 12.3695, 11.9575, 2.95955, 5.82372, 4.31562]

# One distinct color per stage so pie slices and bars match visually
colors = ['#4CAF50', '#2196F3', '#FF5722', '#FF9800', '#9C27B0', '#00BCD4', '#795548']

# Create a figure with two side-by-side subplots
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
fig.suptitle('Canny Pipeline - Phase 5: Hotspot Identification (-O3)', fontsize=14, fontweight='bold')

# Left chart: pie chart showing each stage's share of total runtime
ax1.pie(times, labels=stages, colors=colors, autopct='%1.1f%%', startangle=90)
ax1.set_title('Execution Time Breakdown (7 Stages)')

# Right chart: bar chart showing absolute time per stage in milliseconds
bars = ax2.bar(stages, times, color=colors)
ax2.set_ylabel('Average Time (ms)')
ax2.set_title('Per-Stage Execution Time (-O3)')

# Annotate each bar with its exact time value above the bar
for bar, t in zip(bars, times):
    ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
             f'{t:.2f}ms', ha='center', fontsize=9)

# Rotate x-axis labels so they don't overlap
ax2.set_xticks(range(len(stages)))
ax2.set_xticklabels(stages, rotation=15)

plt.tight_layout()   # auto-adjust spacing so labels don't get clipped
plt.savefig('results/phase5_profile.png', dpi=150, bbox_inches='tight')
print("Saved: results/phase5_profile.png")