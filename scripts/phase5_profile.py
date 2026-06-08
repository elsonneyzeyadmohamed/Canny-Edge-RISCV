import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Phase 5 Results
stages = ['Gaussian\n5x5', 'Sobel\nGx/Gy', 'Magnitude\nL2', 'Direction', 'NMS', 'Double\nThreshold', 'Hysteresis']
times  = [8.9815, 2.76277, 12.3814 , 11.7632, 2.96779, 5.78142, 4.08238] 
colors = ['#4CAF50', '#2196F3', '#FF5722', '#FF9800']

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
fig.suptitle('Canny Pipeline - Phase 5: Hotspot Identification', fontsize=14, fontweight='bold')

# Pie Chart
ax1.pie(times, labels=stages, colors=colors, autopct='%1.1f%%', startangle=90)
ax1.set_title('Execution Time Breakdown')

# Bar Chart
bars = ax2.bar(stages, times, color=colors)
ax2.set_ylabel('Average Time (ms)')
ax2.set_title('Per-Stage Execution Time (-O3)')
for bar, t in zip(bars, times):
    ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
             f'{t:.2f}ms', ha='center', fontsize=9)

plt.tight_layout()
plt.savefig('results/phase5_profile.png', dpi=150, bbox_inches='tight')
print("Saved: results/phase5_profile.png")
