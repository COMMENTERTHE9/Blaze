# 🌊 The Complete Water Metaphor in Blaze

Everything in Blaze is about **water movement**. The `<` and `>` symbols simply show which direction the water flows.

## Core Concept: It's Just Water Direction

- `<` = Water flows ← (from right to left)
- `>` = Water flows → (from left to right)

That's it. Everything else builds on this simple concept.

## 1. River Flow & Timing Operators

| Symbol | Name | Water Movement |
|--------|------|----------------|
| `<` | BEFORE | Upstream flow (happens before) |
| `>` | AFTER | Downstream flow (happens after) |
| `<<` | ONTO | Small inlet feeding in |
| `>>` | INTO | Small outlet flowing out |
| `<>` | BOTH | Whirlpool (flows both ways) |

### Examples
```blaze
# Simple water movement
data > result         # Water carries data downstream to result
result < compute      # Result receives water from upstream compute

# Multiple streams
a >> b >> c          # Water flows through outlets: a→b→c
z << y << x          # Water feeds back through inlets: x→y→z

# Whirlpool
state1 <> state2     # Water swirls between both
```

## 2. Time-Speed Control - Changing the River's Flow Rate

The slash `/` or `\` combined with arrows controls how fast the water flows through time:

| Symbol | Arrow Direction | Slash | Meaning |
|--------|----------------|--------|---------|
| `>/>` | → future | `/` fast | Fast-forward (speed up time) |
| `>\>` | → future | `\` slow | Slow-motion forward |
| `</<` | ← past | `/` fast | Rewind fast |
| `<\<` | ← past | `\` slow | Slow-motion rewind |

**Mnemonic**: 
- Forward slash `/` = fast (like rushing water)
- Backslash `\` = slow (like water trickling)

### Examples
```blaze
>/>(10)              # Water rushes 10× faster forward
>\>(5)               # Water trickles forward at 1/5 speed
</<(3)               # Water rushes 3× faster backward
<\<(2)               # Water trickles backward at 1/2 speed
```

## 3. Ocean Metaphor - Beyond Rivers

### Programming Concepts as Ocean Features

| Code Concept | Ocean Metaphor |
|--------------|----------------|
| **Method** | The ocean body itself (contains many currents) |
| **Function** | Individual current within the ocean |
| **Class** | Blueprint for a current system (like Gulf Stream) |
| **Classified** | Restricted waters (marked on nautical charts) |
| **Object** | Actual patch of ocean where currents flow |

### Example
```blaze
classified class OceanGyre {
    |pull| circulate.can/{@param:temperature}< :>
        do/ water > north_current \
    
    |push| shear.can/{@param:wind_force}< :>
        do/ south_current < water \
}
```

## 4. Other Uses of < and >

### Function Header Separator
```blaze
|function| verb.can< :>    # Not water flow, just syntax
    do/ actual > flow \    # This is water flow
```

### Comparison Operators (always with *)
```blaze
f.if/{@param:x} *>10>     # Not water flow, it's "greater than"
f.chk/{@param:y} *<5>     # Not water flow, it's "less than"
```

### Connectors
```blaze
\>|     # Forward connector (not water)
\<|     # Backward connector (not water)
```

## The Key Insight

When you see `<` or `>` without special markers:
- **It's always about water direction**
- Left-to-right = downstream (>)
- Right-to-left = upstream (<)
- Double arrows = stronger flow (>> or <<)
- Both ways = whirlpool (<>)

Everything else (comparisons, syntax markers) has additional symbols to distinguish from pure water flow.

## Visual Summary

```
        UPSTREAM                    DOWNSTREAM
           ←                           →
    <<<<<<<<<<<<<<<<        >>>>>>>>>>>>>>>>
           ←                           →
        
    source < river < delta    ocean > shore > beach
           ←       ←                 →       →
           
              <> whirlpool <>
               ↔           ↔
```

In Blaze, your data is water, and your program is the landscape it flows through!