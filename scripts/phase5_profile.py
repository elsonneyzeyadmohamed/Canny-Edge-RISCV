import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

stages = ['Gaussian\n5x5', 'Sobel\nGx/Gy', 'Magnitude\nL2', 'Direction',
          'NMS', 'Double\nThreshold', 'Hysteresis']
times  = [9.23, 2.99, 13.11, 12.24, 4.30, 6.00, 4.43]
colors = ['#4CAF50', '#2196F3', '#FF5722', '#FF9800', '#9C27B0', '#00BCD4', '#795548']

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
fig.suptitle('Canny Pipeline - Phase 5: Hotspot Identification', fontsize=14, fontweight='bold')

ax1.pie(times, labels=stages, colors=colors, autopct='%1.1f%%', startangle=90)
ax1.set_title('Execution Time Breakdown (7 Stages)')

bars = ax2.bar(stages, times, color=colors)
ax2.set_ylabel('Average Time (ms)')
ax2.set_title('Per-Stage Execution Time (-O3)')
for bar, t in zip(bars, times):
    ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
             f'{t:.2f}ms', ha='center', fontsize=9)
plt.xticks(rotation=15)

plt.tight_layout()
plt.savefig('results/phase5_profile.png', dpi=150, bbox_inches='tight')
print("Saved: results/phase5_profile.png")
