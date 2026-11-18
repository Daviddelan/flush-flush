# Analyse HPC data and compare Flush+Reload and Flush+Flush detectability.

import pandas as pd
import matplotlib.pyplot as plt

# Check for missing values and invalid instruction counts.
def check_data(data):
    if data.isna().sum().sum() != 0:
        raise ValueError("The CSV file contains missing values.")
    if (data["instructions"] <= 0).any():
        raise ValueError("Invalid instructions count.")

# Calculate sampling window times and check counter running status.
def process_time(data):
    data["window_enabled"] = data["time_enabled"].diff()
    data["window_enabled"] = data["window_enabled"].fillna(data["time_enabled"]).astype("int64")
    data["window_running"] = data["time_running"].diff()
    data["window_running"] = data["window_running"].fillna(data["time_running"]).astype("int64")

    if (data["window_enabled"] <= 0).any():
        raise ValueError("window_enabled data error")
    elif (data["window_running"] <= 0).any():
        raise ValueError("window_running data error")
    elif (data["window_running"] > data["window_enabled"]).any():
        raise ValueError("window_running > window_enabled")
    if(data["window_running"] < data["window_enabled"]).any():
        print("Multiplexing warning")

    data["running_ratio"] = data["window_running"] / data["window_enabled"]

# Normalize cache misses and references per million instructions.
def normalize_hpc(data):
    data["misses_per_million"] = data["cache_misses"] / data["instructions"] * 1000000
    data["references_per_million"] = data["cache_references"] / data["instructions"] * 1000000

# Calculate mean, standard deviation, and coefficient of variation.
def calculate_stats(data):
    mean_mpm = data["misses_per_million"].mean()
    mean_rpm = data["references_per_million"].mean()
    std_mpm = data["misses_per_million"].std()
    std_rpm = data["references_per_million"].std()

    cv_mpm = std_mpm / mean_mpm
    cv_rpm = std_rpm / mean_rpm

    return mean_mpm, mean_rpm, std_mpm, std_rpm, cv_mpm, cv_rpm

# Calculate the number and proportion of flagged samples.
def detection_rate(data):
    flagged_count = data["suspicious_mpm"].sum()
    flagged_rate = flagged_count / len(data)

    return flagged_count, flagged_rate

df = pd.read_csv("baseline_train.csv")
df_test = pd.read_csv("baseline_test.csv")
df_fr = pd.read_csv("flush_reload.csv")
df_ff = pd.read_csv("flush_flush.csv")
df_base_run1 = pd.read_csv("baseline_run1.csv")
df_base_run2 = pd.read_csv("baseline_run2.csv")
df_base_run3 = pd.read_csv("baseline_run3.csv")
df_fr_run1 = pd.read_csv("runs/fr_run1.csv")
df_fr_run2 = pd.read_csv("runs/fr_run2.csv")
df_fr_run3 = pd.read_csv("runs/fr_run3.csv")
df_ff_run1 = pd.read_csv("runs/ff_run1.csv")
df_ff_run2 = pd.read_csv("runs/ff_run2.csv")
df_ff_run3 = pd.read_csv("runs/ff_run3.csv")


base_runs = [df_base_run1, df_base_run2, df_base_run3]
fr_runs = [df_fr_run1, df_fr_run2,df_fr_run3]
ff_runs = [df_ff_run1, df_ff_run2, df_ff_run3]
all_data = [df, df_test, df_fr, df_ff] + base_runs + fr_runs + ff_runs
for data in all_data:
    check_data(data)
    process_time(data)
    normalize_hpc(data)

for i, data in enumerate(base_runs, start=1):
    print(f"BASE Run {i}: {data['misses_per_million'].mean():.2f}")

for i, data in enumerate(fr_runs, start=1):
    print(f"FR Run {i}: {data['misses_per_million'].mean():.2f}")

for i, data in enumerate(ff_runs, start=1):
    print(f"FF Run {i}: {data['misses_per_million'].mean():.2f}")

mean_mpm, mean_rpm, std_mpm, std_rpm, cv_mpm, cv_rpm = calculate_stats(df)
mean_fr_mpm, mean_fr_rpm, std_fr_mpm, std_fr_rpm, cv_fr_mpm, cv_fr_rpm = calculate_stats(df_fr)
mean_ff_mpm, mean_ff_rpm, std_ff_mpm, std_ff_rpm, cv_ff_mpm, cv_ff_rpm = calculate_stats(df_ff)

print(f"Mean MPM = {mean_mpm:.2f}, Mean RPM = {mean_rpm:.2f}, Std MPM = {std_mpm:.2f}, Std RPM = {std_rpm:.2f}")
print(f"Mean FR MPM = {mean_fr_mpm:.2f}, Mean FR RPM = {mean_fr_rpm:.2f}, Std FR MPM = {std_fr_mpm:.2f}, Std FR RPM = {std_fr_rpm:.2f}")
print(f"Mean FF MPM = {mean_ff_mpm:.2f}, Mean FF RPM = {mean_ff_rpm:.2f}, Std FF MPM = {std_ff_mpm:.2f}, Std FF RPM = {std_ff_rpm:.2f}")

print(f"cv_mpm = {cv_mpm:.2%}, cv_rpm = {cv_rpm:.2%}")
print(f"cv_fr_mpm = {cv_fr_mpm:.2%}, cv_fr_rpm = {cv_fr_rpm:.2%}")
print(f"cv_ff_mpm = {cv_ff_mpm:.2%}, cv_ff_rpm = {cv_ff_rpm:.2%}")

plt.figure()
plt.plot(df["sample"], df["misses_per_million"])
plt.xlabel("Sample")
plt.ylabel("Cache Misses Per Million Instructions")
plt.title("MPM (Cache Misses)")
plt.grid(True)
plt.tight_layout()
plt.savefig("mpm_plot.png")
plt.close()

plt.figure()
plt.plot(df["sample"], df["references_per_million"])
plt.xlabel("Sample")
plt.ylabel("Cache References Per Million Instructions")
plt.title("RPM (Cache References)")
plt.grid(True)
plt.tight_layout()
plt.savefig("rpm_plot.png")
plt.close()

# Calculate the MPM detection threshold using baseline training data.
k = 2
threshold_mpm = mean_mpm + k * std_mpm

for data in all_data:
    data["suspicious_mpm"] = data["misses_per_million"] > threshold_mpm

fp_df_test, FPR = detection_rate(df_test)
tp_fr, tpr_fr = detection_rate(df_fr)
tp_ff, tpr_ff = detection_rate(df_ff)
fnr_fr = 1 - tpr_fr
fnr_ff = 1 - tpr_ff

print(f"FPR = {FPR:.2%}")
print(f"MPM Threshold = {threshold_mpm:.2f}")
print(f"TPR FR = {tpr_fr:.2%}, FNR FR = {fnr_fr:.2%}")
print(f"TPR FF = {tpr_ff:.2%}, FNR FF = {fnr_ff:.2%}")

# Evaluate detection rates across three independent runs for each workload.
for i, data in enumerate(base_runs, start=1):
    count, rate = detection_rate(data)
    print(f"BASE Run {i}: FPR = {rate:.2%}")

for i, data in enumerate(fr_runs, start=1):
    count, rate = detection_rate(data)
    print(f"FR Run {i}: TPR = {rate:.2%}, FNR = {1-rate:.2%}")

for i, data in enumerate(ff_runs, start=1):
    count, rate = detection_rate(data)
    print(f"FF Run {i}: TPR = {rate:.2%}, FNR = {1-rate:.2%}")

plt.figure()
plt.plot(df_test["sample"], df_test["misses_per_million"], marker = "o", label = "Baseline Test")
plt.plot(df_fr["sample"], df_fr["misses_per_million"], marker = "s", label = "Flush+Reload")
plt.plot(df_ff["sample"], df_ff["misses_per_million"], marker = "^", label = "Flush+Flush")
plt.xlabel("Sample")
plt.ylabel("Cache Misses Per Million Instructions")
plt.title("MPM Comparison")
plt.grid(True)

plt.axhline(y=threshold_mpm, linestyle="--", label="MPM Threshold")

plt.yscale("log")
plt.legend()

plt.tight_layout()
plt.savefig("mpm_comparison.png")
plt.close()

plt.figure()
plt.plot(df_test["sample"], df_test["references_per_million"], marker = "o", label = "Baseline Test")
plt.plot(df_fr["sample"], df_fr["references_per_million"], marker = "s", label = "Flush+Reload")
plt.plot(df_ff["sample"], df_ff["references_per_million"], marker = "^", label = "Flush+Flush")
plt.xlabel("Sample")
plt.ylabel("Cache References Per Million Instructions")
plt.title("RPM Comparison")
plt.grid(True)

plt.yscale("log")
plt.legend()

plt.tight_layout()
plt.savefig("rpm_comparison.png")
plt.close()
