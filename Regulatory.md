# RF Regulatory and Usage Notes

This project can transmit/receive in the 433 MHz ISM band. RF regulations vary by country; you are responsible for compliant operation.

## Common Regions (Non-Exhaustive)
- EU/ETSI (e.g., 433.05–434.79 MHz): Low-power, duty-cycle and ERP limits apply. Check EN 300 220 and national implementations.
- US (FCC Part 15): 433.92 MHz operation is permitted under certain sections (e.g., 15.231/15.231e for periodic transmitters). Power, field strength, and timing limits apply.
- Other regions: Bands and limits differ—consult your local authority guidance.

## Best Practices
- Keep transmit power to the minimum required.
- Use appropriate antennas and ensure good filtering to limit harmonics.
- Observe duty-cycle or transmission-time limits.
- Avoid interfering with other users and services.
- Consider adding a low-pass filter if your module does not include one.

This document is not legal advice. Always verify current rules for your jurisdiction before operating RF equipment.
