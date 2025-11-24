#!/usr/bin/env python3
"""
Profile Data Visualization Tool for PgEngine
Reads profile_data.csv and generates timeline visualizations
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.collections import PatchCollection
import numpy as np
import argparse
import sys
from pathlib import Path


# Color scheme for different categories
CATEGORY_COLORS = {
    'Frame': '#ffffff',
    'Event': '#4CAF50',
    'Command': '#FFC107',
    'System': '#2196F3',
    'Render': '#F44336',
    'Input': '#9C27B0',
}


def load_profile_data(csv_path):
    """Load and parse the profile CSV data"""
    try:
        df = pd.read_csv(csv_path)
        print(f"Loaded {len(df)} intervals from {csv_path}")
        print(f"Frame range: {df['frame'].min()} to {df['frame'].max()}")
        print(f"Categories: {df['category'].unique()}")
        return df
    except FileNotFoundError:
        print(f"Error: Could not find {csv_path}")
        print("Make sure to run the SimpleBoxBouncer example first to generate profile_data.csv")
        sys.exit(1)
    except Exception as e:
        print(f"Error loading CSV: {e}")
        sys.exit(1)


def plot_frame_timeline(df, frame_number, output_file=None):
    """
    Plot a horizontal bar chart showing execution timeline for a single frame
    """
    frame_data = df[df['frame'] == frame_number].copy()

    if frame_data.empty:
        print(f"No data found for frame {frame_number}")
        return

    # Sort by start time
    frame_data = frame_data.sort_values('start_ms')

    # Get frame duration (reconstruct if needed)
    frame_row = frame_data[frame_data['name'] == 'Frame']
    if not frame_row.empty:
        frame_duration = frame_row['duration_ms'].iloc[0]
        frame_start = frame_row['start_ms'].iloc[0]
        # Remove Frame entry for cleaner visualization
        frame_data = frame_data[frame_data['name'] != 'Frame']
    else:
        # Reconstruct frame bounds from events
        frame_start = frame_data['start_ms'].min()
        frame_end = (frame_data['start_ms'] + frame_data['duration_ms']).max()
        frame_duration = frame_end - frame_start

    # Create figure
    fig, ax = plt.subplots(figsize=(14, max(8, len(frame_data) * 0.4)))

    # Plot bars
    y_pos = np.arange(len(frame_data))
    colors = [CATEGORY_COLORS.get(cat, '#888888') for cat in frame_data['category']]

    # Adjust start times relative to frame start
    relative_starts = frame_data['start_ms'] - frame_start

    bars = ax.barh(y_pos, frame_data['duration_ms'], left=relative_starts,
                   color=colors, edgecolor='black', linewidth=0.5)

    # Add duration labels on bars
    for i, (idx, row) in enumerate(frame_data.iterrows()):
        duration = row['duration_ms']
        if duration > frame_duration * 0.05:  # Only show label if bar is wide enough
            ax.text(relative_starts.iloc[i] + duration/2, i, f'{duration:.2f}ms',
                   ha='center', va='center', fontsize=8, fontweight='bold')

    # Customize plot
    ax.set_yticks(y_pos)
    ax.set_yticklabels(frame_data['name'])
    ax.set_xlabel('Time (ms)', fontsize=12)
    ax.set_title(f'Frame {frame_number} Execution Timeline\nTotal Frame Time: {frame_duration:.2f}ms ({1000/frame_duration:.1f} FPS)',
                 fontsize=14, fontweight='bold')

    # Add FPS target lines only if frame duration is close to them
    legend_patches = []
    if frame_duration > 13:  # Only show 60 FPS line if frame is slow enough
        ax.axvline(x=16.67, color='green', linestyle='--', alpha=0.7, linewidth=2, label='60 FPS target (16.67ms)')
    if frame_duration > 28:  # Only show 30 FPS line if frame is slow enough
        ax.axvline(x=33.33, color='orange', linestyle='--', alpha=0.7, linewidth=2, label='30 FPS target (33.33ms)')

    # Add legend for categories
    legend_patches += [mpatches.Patch(color=color, label=cat)
                      for cat, color in CATEGORY_COLORS.items()
                      if cat in frame_data['category'].values]

    if legend_patches:  # Only show legend if there are items
        ax.legend(handles=legend_patches, loc='upper right', fontsize=10)

    ax.grid(axis='x', alpha=0.3)
    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved frame timeline to {output_file}")
    else:
        plt.show()


def plot_frame_time_history(df, num_frames=None, top_n_systems=5, output_file=None):
    """
    Plot frame time over multiple frames to see performance trends
    Args:
        top_n_systems: Number of top systems to show in stacked area (default: 5)
    """
    # Get frame times (or reconstruct from system events)
    frame_times_df = df[df['name'] == 'Frame'].copy()

    if len(frame_times_df) == 0:
        # Reconstruct from system events
        frame_times_grouped = df.groupby('frame')['duration_ms'].sum().reset_index()
        frame_times_grouped.columns = ['frame', 'duration_ms']
        frame_times = frame_times_grouped.copy()
    else:
        frame_times = frame_times_df.sort_values('frame').copy()

    if num_frames:
        frame_times = frame_times.tail(num_frames).copy()

    # Calculate FPS from frame times
    frame_times = frame_times.copy()
    frame_times['fps'] = 1000.0 / frame_times['duration_ms']

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(14, 14))

    # Plot 1: Frame time line graph
    avg_frame_time = frame_times['duration_ms'].mean()
    max_frame_time = frame_times['duration_ms'].max()

    ax1.plot(frame_times['frame'], frame_times['duration_ms'],
            linewidth=2, color='#2196F3', marker='o', markersize=2)

    # Only show FPS reference lines if they're relevant (based on max frame time)
    # Show 60 FPS line only if max frame time is close to or above 13ms
    if max_frame_time > 13:
        ax1.axhline(y=16.67, color='green', linestyle='--', linewidth=2, label='60 FPS (16.67ms)', alpha=0.7)
    # Show 30 FPS line only if max frame time is close to or above 28ms
    if max_frame_time > 28:
        ax1.axhline(y=33.33, color='orange', linestyle='--', linewidth=2, label='30 FPS (33.33ms)', alpha=0.7)

    ax1.fill_between(frame_times['frame'], 0, frame_times['duration_ms'],
                     alpha=0.3, color='#2196F3')

    ax1.set_xlabel('Frame Number', fontsize=12)
    ax1.set_ylabel('Frame Time (ms)', fontsize=12)
    ax1.set_title(f'Frame Time History (Avg: {avg_frame_time:.2f}ms, Max: {max_frame_time:.2f}ms)',
                  fontsize=14, fontweight='bold')
    # Only show legend if there are reference lines
    if max_frame_time > 13:
        ax1.legend(fontsize=10)
    ax1.grid(True, alpha=0.3)

    # Plot 2: FPS over time
    avg_fps = frame_times['fps'].mean()
    ax2.plot(frame_times['frame'], frame_times['fps'],
            linewidth=2, color='#4CAF50', marker='o', markersize=2)
    ax2.axhline(y=avg_fps, color='blue', linestyle='--', linewidth=2,
               label=f'Average FPS: {avg_fps:.1f}', alpha=0.7)
    ax2.fill_between(frame_times['frame'], 0, frame_times['fps'],
                     alpha=0.3, color='#4CAF50')

    ax2.set_xlabel('Frame Number', fontsize=12)
    ax2.set_ylabel('FPS', fontsize=12)
    ax2.set_title(f'FPS Over Time (Avg: {avg_fps:.1f} FPS)', fontsize=14, fontweight='bold')
    ax2.legend(fontsize=10)
    ax2.grid(True, alpha=0.3)

    # Plot 3: Top N system execution times stacked area
    system_data = df[df['category'] == 'System'].copy()

    # Pivot to get systems as columns
    pivot_data = system_data.pivot_table(
        index='frame',
        columns='name',
        values='duration_ms',
        aggfunc='sum',
        fill_value=0
    )

    if not pivot_data.empty:
        # Only use frames that exist in both datasets
        common_frames = pivot_data.index.intersection(frame_times['frame'])
        if len(common_frames) > 0:
            pivot_data_filtered = pivot_data.loc[common_frames]

            # Get top N systems by average execution time
            system_averages = pivot_data_filtered.mean().sort_values(ascending=False)
            top_systems = system_averages.head(top_n_systems).index.tolist()

            # If there are more systems, group the rest as "Other"
            if len(pivot_data_filtered.columns) > top_n_systems:
                other_systems = [col for col in pivot_data_filtered.columns if col not in top_systems]
                pivot_data_filtered['Other Systems'] = pivot_data_filtered[other_systems].sum(axis=1)
                top_systems.append('Other Systems')

            # Plot only top N + Other
            pivot_data_top = pivot_data_filtered[top_systems]

            ax3.stackplot(pivot_data_top.index, *[pivot_data_top[col] for col in pivot_data_top.columns],
                         labels=pivot_data_top.columns, alpha=0.8)
            ax3.set_xlabel('Frame Number', fontsize=12)
            ax3.set_ylabel('System Time (ms)', fontsize=12)
            ax3.set_title(f'Top {top_n_systems} System Execution Times (Stacked)', fontsize=14, fontweight='bold')
            ax3.legend(loc='upper left', fontsize=9, ncol=2)
            ax3.grid(True, alpha=0.3)
        else:
            ax3.text(0.5, 0.5, 'No system data available', ha='center', va='center',
                    transform=ax3.transAxes, fontsize=12)
            ax3.axis('off')

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved frame history to {output_file}")
    else:
        plt.show()


def plot_category_breakdown(df, output_file=None):
    """
    Plot pie charts and bar charts showing time spent in each category
    """
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 12))

    # Average time per category across all frames
    category_times = df.groupby('category')['duration_ms'].mean()
    colors = [CATEGORY_COLORS.get(cat, '#888888') for cat in category_times.index]

    # Pie chart
    ax1.pie(category_times, labels=category_times.index, autopct='%1.1f%%',
           colors=colors, startangle=90)
    ax1.set_title('Average Time per Category', fontsize=12, fontweight='bold')

    # Bar chart - average
    ax2.bar(category_times.index, category_times.values, color=colors, edgecolor='black')
    ax2.set_ylabel('Average Time (ms)', fontsize=10)
    ax2.set_title('Average Category Times', fontsize=12, fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)
    ax2.tick_params(axis='x', rotation=45)

    # System-level breakdown
    system_times = df[df['category'] == 'System'].groupby('name')['duration_ms'].mean().sort_values(ascending=True)

    if not system_times.empty:
        ax3.barh(range(len(system_times)), system_times.values, color='#2196F3', edgecolor='black')
        ax3.set_yticks(range(len(system_times)))
        ax3.set_yticklabels(system_times.index, fontsize=9)
        ax3.set_xlabel('Average Time (ms)', fontsize=10)
        ax3.set_title('System Execution Times', fontsize=12, fontweight='bold')
        ax3.grid(axis='x', alpha=0.3)

        # Add values on bars
        for i, v in enumerate(system_times.values):
            ax3.text(v, i, f' {v:.3f}ms', va='center', fontsize=8)

    # Frame time statistics (reconstruct if needed)
    frame_times_df = df[df['name'] == 'Frame']['duration_ms']

    if len(frame_times_df) == 0:
        # Reconstruct from system events
        frame_times = df.groupby('frame')['duration_ms'].sum()
        note = "\n(Reconstructed from system events)\n"
    else:
        frame_times = frame_times_df
        note = "\n"

    stats_text = f"""
Frame Time Statistics:
━━━━━━━━━━━━━━━━━━━━{note}Total Frames: {len(frame_times)}
Average:  {frame_times.mean():.2f} ms
Median:   {frame_times.median():.2f} ms
Min:      {frame_times.min():.2f} ms
Max:      {frame_times.max():.2f} ms
Std Dev:  {frame_times.std():.2f} ms

P50:      {frame_times.quantile(0.50):.2f} ms
P95:      {frame_times.quantile(0.95):.2f} ms
P99:      {frame_times.quantile(0.99):.2f} ms

Avg FPS:  {1000/frame_times.mean():.1f}
    """

    ax4.text(0.1, 0.5, stats_text, fontsize=11, family='monospace',
            verticalalignment='center', transform=ax4.transAxes)
    ax4.axis('off')
    ax4.set_title('Performance Statistics', fontsize=12, fontweight='bold')

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved category breakdown to {output_file}")
    else:
        plt.show()


def plot_all_frames_timeline(df, max_frames=100, output_file=None):
    """
    Plot a heatmap/timeline showing all frames
    Useful for spotting performance spikes
    """
    # Get unique systems
    systems = df[df['category'] == 'System']['name'].unique()

    # Create matrix: frames x systems
    frame_range = sorted(df['frame'].unique())[:max_frames]

    matrix = np.zeros((len(systems), len(frame_range)))

    for i, system in enumerate(systems):
        for j, frame in enumerate(frame_range):
            time = df[(df['frame'] == frame) & (df['name'] == system)]['duration_ms'].sum()
            matrix[i, j] = time

    fig, ax = plt.subplots(figsize=(16, max(8, len(systems) * 0.3)))

    im = ax.imshow(matrix, aspect='auto', cmap='YlOrRd', interpolation='nearest')

    ax.set_yticks(range(len(systems)))
    ax.set_yticklabels(systems, fontsize=9)
    ax.set_xlabel('Frame Number', fontsize=12)
    ax.set_title(f'System Execution Heatmap (First {len(frame_range)} frames)',
                fontsize=14, fontweight='bold')

    # Colorbar
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Execution Time (ms)', fontsize=10)

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved timeline heatmap to {output_file}")
    else:
        plt.show()


def print_summary(df):
    """Print text summary of profile data"""
    print("\n" + "="*60)
    print("PROFILE SUMMARY")
    print("="*60)

    # Try to get Frame times, or reconstruct from per-frame totals
    frame_times = df[df['name'] == 'Frame']['duration_ms']

    if len(frame_times) == 0:
        # Reconstruct frame times from system events
        print("\nNote: Frame times reconstructed from system events")
        frame_times_reconstructed = df.groupby('frame')['duration_ms'].sum()
        frame_times = frame_times_reconstructed
        total_frames = len(frame_times)
    else:
        total_frames = len(frame_times)

    print(f"\nTotal Frames Captured: {total_frames}")
    print(f"Frame Range: {df['frame'].min()} - {df['frame'].max()}")
    print(f"\nFrame Time Statistics:")

    if len(frame_times) > 0:
        print(f"  Average: {frame_times.mean():.3f} ms ({1000/frame_times.mean():.1f} FPS)")
        print(f"  Median:  {frame_times.median():.3f} ms")
        print(f"  Min:     {frame_times.min():.3f} ms")
        print(f"  Max:     {frame_times.max():.3f} ms")
        print(f"  Std Dev: {frame_times.std():.3f} ms")
    else:
        print("  No timing data available")

    print(f"\nCategory Breakdown (Average per Frame):")
    category_avg = df.groupby('category')['duration_ms'].mean().sort_values(ascending=False)
    for cat, time in category_avg.items():
        print(f"  {cat:12s}: {time:6.3f} ms")

    print(f"\nTop 5 Slowest Systems (Average):")
    system_avg = df[df['category'] == 'System'].groupby('name')['duration_ms'].mean().sort_values(ascending=False).head(5)
    for i, (sys, time) in enumerate(system_avg.items(), 1):
        print(f"  {i}. {sys:30s}: {time:6.3f} ms")

    # Find worst frame
    if len(frame_times) > 0:
        worst_frame_idx = frame_times.idxmax()
        worst_time = frame_times.max()

        # If reconstructed, worst_frame_idx IS the frame number
        if len(df[df['name'] == 'Frame']) == 0:
            worst_frame = worst_frame_idx
        else:
            worst_frame = df[df.index == worst_frame_idx]['frame'].iloc[0]

        print(f"\nWorst Frame: #{worst_frame} ({worst_time:.3f} ms, {1000/worst_time:.1f} FPS)")

    print("="*60 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Visualize PgEngine profiler data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Show all visualizations with default settings
  python visualize_profile.py

  # Show specific frame timeline
  python visualize_profile.py --frame 100

  # Show last 1000 frames with top 3 systems
  python visualize_profile.py --num-frames 1000 --top-systems 3

  # Show frames from a specific range
  python visualize_profile.py --start-frame 1000 --end-frame 2000

  # Save plots to files instead of displaying
  python visualize_profile.py --output

  # Show top 10 systems in stacked area chart
  python visualize_profile.py --top-systems 10

  # Use custom CSV file
  python visualize_profile.py --input my_profile.csv
        """
    )

    parser.add_argument('--input', '-i', default='profile_data.csv',
                       help='Input CSV file (default: profile_data.csv)')
    parser.add_argument('--frame', '-f', type=int,
                       help='Show timeline for specific frame number')
    parser.add_argument('--output', '-o', action='store_true',
                       help='Save plots to files instead of showing')
    parser.add_argument('--num-frames', '-n', type=int, default=1000,
                       help='Number of frames to show in history plots (default: 1000)')
    parser.add_argument('--start-frame', type=int,
                       help='Start frame for analysis range')
    parser.add_argument('--end-frame', type=int,
                       help='End frame for analysis range')
    parser.add_argument('--top-systems', '-t', type=int, default=5,
                       help='Number of top systems to show in stacked area chart (default: 5)')

    args = parser.parse_args()

    # Load data
    df = load_profile_data(args.input)

    # Apply frame range filtering if specified
    if args.start_frame is not None or args.end_frame is not None:
        start = args.start_frame if args.start_frame is not None else df['frame'].min()
        end = args.end_frame if args.end_frame is not None else df['frame'].max()
        df = df[(df['frame'] >= start) & (df['frame'] <= end)]
        print(f"\nFiltered to frame range: {start} - {end}")
        print(f"Remaining data: {len(df)} intervals")

    # If dataset is too large, warn and suggest using frame range
    total_frames = df['frame'].max() - df['frame'].min() + 1
    if total_frames > 5000 and args.start_frame is None and args.end_frame is None:
        print(f"\n⚠ Warning: Dataset contains {total_frames} frames!")
        print(f"  For better visualization, consider using --start-frame and --end-frame")
        print(f"  Example: --start-frame {df['frame'].max() - 1000} --end-frame {df['frame'].max()}")

        # Auto-limit to last frames if not specified
        if not args.frame:
            max_frame = df['frame'].max()
            min_frame = max(df['frame'].min(), max_frame - args.num_frames)
            df = df[df['frame'] >= min_frame]
            print(f"\n  Auto-limiting to last {args.num_frames} frames ({min_frame} - {max_frame})")

    # Print summary
    print_summary(df)

    if args.frame:
        # Show specific frame
        output = f'frame_{args.frame}_timeline.png' if args.output else None
        plot_frame_timeline(df, args.frame, output)
    else:
        # Show all plots
        if args.output:
            print("\nGenerating plots...")

            # Pick a representative frame (median frame time)
            frame_times_df = df[df['name'] == 'Frame'][['frame', 'duration_ms']]

            if len(frame_times_df) == 0:
                # Reconstruct from system events
                frame_times_series = df.groupby('frame')['duration_ms'].sum()
                median_time = frame_times_series.median()
                median_frame = frame_times_series.iloc[(frame_times_series - median_time).abs().argsort()[:1]].index[0]
            else:
                median_time = frame_times_df['duration_ms'].median()
                median_frame = frame_times_df.iloc[(frame_times_df['duration_ms'] - median_time).abs().argsort()[:1]]['frame'].iloc[0]

            plot_frame_timeline(df, median_frame, 'frame_timeline.png')
            plot_frame_time_history(df, args.num_frames, args.top_systems, 'frame_history.png')
            plot_category_breakdown(df, 'category_breakdown.png')
            plot_all_frames_timeline(df, max_frames=100, output_file='timeline_heatmap.png')

            print("\n✓ All plots saved!")
            print("  - frame_timeline.png")
            print("  - frame_history.png")
            print("  - category_breakdown.png")
            print("  - timeline_heatmap.png")
        else:
            # Interactive display
            print("\nShowing interactive plots (close each window to see the next)...\n")

            # Pick median frame
            frame_times_df = df[df['name'] == 'Frame'][['frame', 'duration_ms']]

            if len(frame_times_df) == 0:
                # Reconstruct from system events
                frame_times_series = df.groupby('frame')['duration_ms'].sum()
                median_time = frame_times_series.median()
                median_frame = frame_times_series.iloc[(frame_times_series - median_time).abs().argsort()[:1]].index[0]
            else:
                median_time = frame_times_df['duration_ms'].median()
                median_frame = frame_times_df.iloc[(frame_times_df['duration_ms'] - median_time).abs().argsort()[:1]]['frame'].iloc[0]

            print(f"1/4: Showing timeline for frame {median_frame} (median frame time)...")
            plot_frame_timeline(df, median_frame)

            print("2/4: Showing frame time history and FPS...")
            plot_frame_time_history(df, args.num_frames, args.top_systems)

            print("3/4: Showing category breakdown...")
            plot_category_breakdown(df)

            print("4/4: Showing timeline heatmap...")
            plot_all_frames_timeline(df, max_frames=100)


if __name__ == '__main__':
    main()
