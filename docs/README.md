# Documentation

This folder contains project documentation.

## Contents

- `Project_Report.pdf` - Detailed project report (add your report here)

## Report Should Include

Based on the project scope, your report should cover:

1. **Introduction & Objectives**
2. **Literature Review** (existing assistive technologies)
3. **System Design & Architecture**
4. **Hardware Components** (detailed specifications)
5. **Software Implementation**
6. **Power Analysis & Battery Recommendations**
7. **Testing & Results**
8. **Limitations & Future Work**
9. **Conclusion**
10. **References**

## Power Analysis Notes

### ESP32-S3 Power Consumption (Estimated)

| Mode | Current |
|------|---------|
| WiFi Active | ~150-200mA |
| Camera Active | ~100mA |
| TOF Sensor | ~20mA |
| **Total (Active)** | ~300-350mA |

### ESP32-C3 Power Consumption (Estimated)

| Mode | Current |
|------|---------|
| WiFi Active | ~100-150mA |
| Motor ON | ~50-100mA |
| **Total (Active)** | ~150-250mA |

### Battery Recommendations

| Device | Battery | Capacity | Est. Runtime |
|--------|---------|----------|--------------|
| Eyewear (S3) | 3.7V LiPo | 2000mAh | ~4-6 hours |
| Handband (C3) | 3.7V LiPo | 1000mAh | ~4-6 hours |