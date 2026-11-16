# LuxAnalytics: Luxury Clienteling & RFM Analytics Engine

A zero-dependency, high-speed C++17 command-line library designed to segment luxury retail shoppers into actionable clienteling tiers (e.g., *Ultra-VIC*, *Rising Collector*, *At-Risk Patron*) using weighted RFM scoring and custom 3D K-Means clustering.

---

## 1. Motivation
Standard retail analytics break down in luxury fashion, fine jewelry, and haute horlogerie due to three industry realities:
* **Extreme Monetary Skew:** Purchase values range from $300 accessories to $50,000+ timepieces, distorting simple average spend metrics.
* **Longer Purchase Cycles & Quiet Churn:** Luxury clients do not shop weekly. High-value patrons who quietly lapse must be flagged early before defecting to rival houses.
* **Clienteling Over Discounting:** Luxury maisons never send mass discount coupons; boutique advisors require targeted outreach recommendations (e.g., *Private Salon Invitation*, *Limited-Edition Quota Allocation*, *Personal Stylist Outreach*).

## 2. Innovation
Unlike static rule-based scripts or heavy external Python/BI dependencies, **LuxAnalytics** provides a reusable, standalone C++ engine featuring:
* **From-Scratch 3D K-Means Clustering (`KMeansCluster`):** Dynamically normalizes customer `(Recency, Frequency, Monetary)` vectors and groups shoppers into behavioral clusters without hardcoded spend thresholds.
* **Configurable Luxury Weighting (`RFMCalculator`):** Allows boutique managers to tune $W_R, W_F, W_M$ weights via CLI flags to prioritize high-ticket spenders or recent boutique visitors.
* **Millisecond Batch Processing:** Processes thousands of boutique transaction records locally with zero third-party libraries.

## 3. Setup & Build Guide

### Prerequisites
* A C++17 compatible compiler (`g++` or `clang++`)
* `make` (optional)

### Compilation
Clone the repository and compile using `make` or `g++`:
```bash
git clone [https://github.com/subyslair/LuxAnalytics.git](https://github.com/subyslair/LuxAnalytics.git)
cd LuxAnalytics
make
