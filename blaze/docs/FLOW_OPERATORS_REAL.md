# 🌊 Blaze Flow Operators - The River Model

Blaze execution flows like a river - and can flow BACKWARDS in time!

## Core Flow Concept

Data in Blaze flows like water through your program:
```
input → processing → time_travel_feedback → decisions → output
```

But unlike traditional languages, this river can flow backwards!

## Flow Direction Operators

### Basic Flow Markers

```blaze
< = flows FROM right TO left (backward/before)
> = flows FROM left TO right (forward/after)
<< = ONTO target (affects first)
>> = INTO target (outputs to)
```

### Forward Flow Examples (Downstream)
```blaze
# Data flows forward through processing
data.process< input< do/ transform/ continue\

# Simple forward flow
value > variable        # value flows INTO variable
a > b > c              # Chain flow: a flows to b, b flows to c
```

### Backward Flow Examples (Upstream)
```blaze
# Results flow back in time
< store >> result/     # Result flows back to store

# Backward assignment
variable < computation  # Computation flows back to variable
```

### Pipeline Flow
```blaze
# Forward pipeline
input >> process >> output    # Data flows through pipeline

# Backward pipeline  
output << process << input    # Results flow back through stages
```

## Branching Flow (River Splits)

When execution needs to branch like a river delta:

```blaze
fucn.ck/condition *>true> branch_a/ condition*_<true > branch_b< \>|
```

Breaking this down:
- `fucn.ck/condition` - Check condition
- `*>true>` - If true, flow goes right to branch_a
- `*_<true >` - If false, flow goes to branch_b
- The flow literally splits based on the condition!

## Timeline Flow Operators

Blaze can jump through time:

```blaze
^timeline.[target]/    # Jump TO target point in time
timeline-[target]      # Define target location in timeline

# Example: Save point and rewind
timeline-[before_risky_operation]
# ... do risky stuff ...
if/ failed <
    ^timeline.[before_risky_operation]/  # Jump back!
:>
```

## Bidirectional Flow `</<`

For quantum operations where data flows both ways simultaneously:

```blaze
a </< b    # a and b exchange data bidirectionally
           # Like quantum entanglement!
```

## The `/` and `\` - NOT Flow Operators!

Important clarification:
- `/` is just a command separator: `print/ "hello"`
- `\` is just a statement terminator: `print/ "hello" \`

They're syntax markers, NOT flow operators!

## Real Examples

### Data Processing Pipeline
```blaze
# Forward flow through transformations
raw_input >> validate >> transform >> optimize >> final_result

# Or backward flow for reverse engineering
final_result << optimize << transform << validate << raw_input
```

### Time-Aware Conditional
```blaze
timeline-[start]
var.v-attempt-[0]

|try_operation| verb.can<
    attempt = attempt + 1
    if/ attempt > 3 <
        print/ "Too many attempts!" \
    :> else <
        if/ ^risky_operation/ *>fails> <
            ^timeline.[start]/    # Flow back in time!
        :>
    :>
:>
```

### Quantum State Exchange
```blaze
# Two variables in quantum superposition
var.q-state1-["alive"]
var.q-state2-["dead"]

# Bidirectional flow collapses the states
state1 </< state2    # Now they're entangled!
```

## The River Mental Model

Think of your program as a river system:
- Normal flow goes downstream (>)
- Time travel flows upstream (<)
- Branches split the river (*>condition>)
- Pipelines are rapids (>>)
- Bidirectional flow is a whirlpool (</<)

Unlike traditional "railroad track" execution, Blaze flows like water - finding its path, splitting when needed, even flowing backwards when time travel is required!