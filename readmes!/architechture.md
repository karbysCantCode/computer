# Notes

## 1.X - Decoding and register reading

  ### 1.1 - Disable MSB of intra-instruction 6 bit immediate.
  
  This forces the MSB of the 6 bit immediate, from the LSB of the instruction, to be zero.  
  This effectively makes it a 5 bit immediate, and is only used to save on extending the mux that determines the source of an intra-instruction immediate. (as there are already 4 sources - full 10 LSB, 4 MSB (ex opcode), 3 LSB, and 6 LSB).

  ### 1.2 - Field clarification

  N1 -> 'Normal' format field one  (4 bits: OOOOOO NNNN XXX XXX).
  N2 -> 'Normal' format field two  (3 bits: OOOOOO XXXX NNN XXX).
  N3 -> 'Normal' format field three (3 bits: OOOOOO XXXX XXX NNN).
  2A -> 'Two' format field A  (5 bits: OOOOOO NNNNN XXXXX).
  2B -> 'Two' format field B  (5 bits: OOOOOO XXXXX NNNNN).

  ### 1.3 - Next instruction - Hardware interrupt & Injecting Instruction

  This is here because, if an interrupt occurs on the first half of an instruction that injects, hardware interrupt should NOT assert 'injecting instruction', convieniently they both use the same priority here. This explanation kinda sucks but this is a feature in the works so I don't even know how it works yet.
    
## 2.X - Execution and (PC?)

## 3.X - Memory

### FOR DEVICES AND DMA, SEE 5.X.

### 3.1 - Ram Data Moving Toggling

This is because (I believe) the caches will start storing data when requesting and ram data moving are high. So if the both caches are requesting, and because of arbitration, only one of them will currently have access to the memory, when one caches request is fufilled, and the controller pulls ram data moving high, (if the signal wasnt toggled) both caches would begin storing that data, at the individual address they are requesting (which if different - which is most likely - would cause incorrect data to be written to caches.)  
Hence the toggling to the current bus master.

### 3.2 - Data cache RWDS assert

I'm pretty sure this is because RWDS should only be asserted by the data cache when writing, because it is driven by the controller when reading. My only concern is in setup, if RWDS is driven to indicate the waiting period of the DRAM ICS, this may conflict.

### 3.3 - Instruction fetch virtual mode

I think this is only as an optimisation for startup performance, where translation and passthrough are disabled so that the instruction cache doesn't stall the system trying to fetch data it won't even use (because instructions are being read from the startup cache). So this may not actually be needed, and could be a point for saving components, just at the cost of slower startup. (but it's startup, it's literally negligible.)



## 4.X - Writeback

## 5.X - DMA and Devices