# CAN capture CSV format

The initial replay tool accepts:

```csv
timestamp_ms,bus,id,data
1000,C1,0x4B2,00 28 10 00 00 00 00 00
1010,C1,0x2ED,00 00 00 00 00 00 01 00
```

Fields:

- `timestamp_ms`: monotonic milliseconds
- `bus`: C1, C2 or BH
- `id`: decimal or `0x` hexadecimal
- `data`: hexadecimal bytes separated by spaces
