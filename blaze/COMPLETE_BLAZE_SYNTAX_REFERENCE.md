# COMPLETE BLAZE LANGUAGE SYNTAX REFERENCE
## Every Syntax Element for Claude Code Implementation

---

## CORE PUNCTUATION SYSTEM

### Foundation Rules
```
DASH (-)     = NAMING/IDENTIFICATION
UNDERSCORE (_) = CONNECTION (no execution)
DOT (.)      = ACTION/EXECUTION
```

---

## IDENTIFIERS AND NAMING

### Pipe Notation
```blaze
|identifier_name|        # Function/variable/object names
|function_name|          # Function identifier
|variable_name|          # Variable identifier
|object_name|            # Object identifier
```

### Variable Declaration
```blaze
var.v-variable_name-[value]
var.v-name-[42]
var.v-data-[hello;world;test]
```

### Function Declaration
```blaze
|function_name| method.can/{@param:parameter}< :>
|process| math.can/{@param:x}/{@param:y}< :>
```

### Function Header Separator
```blaze
< :>         # Separates function header from body
             # Ends |name| verb.can... header, opens do/...\ body
```

---

## BLUEPRINTS AND OBJECTS

### Blueprint Declaration
```blaze
blueprint.b-name-[properties]      # Declare a class/blueprint
blueprint.b-car-[make;model;year]
blueprint.b-person-[name;age;role]
```

### Object Instantiation
```blaze
object.o-id-[blueprint]           # Instantiate blueprint
object.o-mycar-[car]
object.o-user1-[person]
```

### Method Declarations (Inside Blueprints)
```blaze
method.m-name verb.can/{@param:...}< :>   # Methods inside blueprints
method.m-drive accelerate.can/{@param:speed}< :>
method.m-update modify.can/{@param:field}/{@param:value}< :>
```

---

## PARAMETER SYSTEM

### Parameter Syntax
```blaze
/{@param:parameter_name}
/{@param:first}/{@param:second}
/{@param:data[item1;item2;item3]}
```

### Parameter Examples
```blaze
function_name/{@param:value}
gap.compute/{@param:metrics}/{@param:missing[cpu;memory]}
print/{@param:"Hello World"}
```

---

## BRACKETS AND DELIMITERS

### Bracket Types
```blaze
[]           # Arrays, values, parameters
{}           # Parameters only: {@param:name}
< >          # Timing operators, method calls
()           # NOT USED in Blaze
```

### Bracket Usage
```blaze
[85;92;78]                    # Array values
{@param:name}                 # Parameters
method.can< parameter<        # Method calls
```

---

## TIMING OPERATORS

### Flow Control
```blaze
<            # BEFORE (upstream)
>            # AFTER (downstream)
<<           # ONTO (tributary in)
>>           # INTO (flow out)
<>           # BOTH (before and after)
>/>          # BRIDGE (jump over)
```

### Time-Rate Control Operators
```blaze
>/> (n)      # Fast-forward ×n speed
>\> (n)      # Slow-mo forward (1/n speed)
</< (n)      # Fast rewind ×n speed
<\< (n)      # Slow rewind (1/n speed)
||           # Pause timeline
```

### Timing Examples
```blaze
< store >> result/           # Time travel storage
input<< process >> output    # Data flow
operation<> validation       # Before and after
start >/> end               # Bridge over middle
>/> (10)                    # Fast-forward 10x
<\< (2)                     # Slow rewind at half speed
||                          # Pause execution
```

---

## ACTION BLOCKS

### Structure
```blaze
do/          # Start action block
/            # Continue action
\            # End action block
```

### Examples
```blaze
do/ action/ continue/ more\
do/ load/ process/ save\
do/ validate/ < store >> result/ finalize\
```

---

## CONDITIONALS

### Conditional Functions
```blaze
f.{abbreviation}             # Short form
fucn.{abbreviation}          # Full form
```

### All Conditional Abbreviations
```blaze
f.ens       # ensure
f.ver       # verify
f.chk       # check
f.try       # try
f.grd       # guard
f.unl       # unless
f.if        # if
f.whl       # while
f.unt       # until
f.obs       # observe
f.det       # detect
f.rec       # recover
f.fs        # failsafe
f.rte       # route
f.mon       # monitor
f.eval      # evaluate
f.dec       # decide
f.ass       # assess
f.msr       # measure
```

### Conditional Syntax
```blaze
f.{abbrev}/{@param:var} *{comparison}{value}> action/ var*_{comparison}{value} > fallback< \>|
```

### Conditional Examples
```blaze
f.chk/{@param:data} *>valid> process/ data*_<valid > reject< \>|
f.ens/{@param:safety} *>ok> proceed/ safety*_<ok > stop< \>|
```

---

## COMPARISON OPERATORS

### Comparison Syntax
```blaze
*>value      # Greater than
*_<value     # Less than or equal to
*_>value     # Greater than or equal to
*=value      # Equal to
*!=value     # Not equal to
```

### Comparison Examples
```blaze
*>30         # Greater than 30
*_<50        # Less than or equal to 50
*=100        # Equal to 100
```

---

## CONNECTORS

### Flow Connectors
```blaze
\>|          # Forward connector
\<|          # Backward connector
```

### Chaining Connectors
```blaze
._           # Chain actions with underscore
recv._merg   # Receive and merge
load._save   # Load and save
```

### Connector Examples
```blaze
condition *>true> action/ \>|         # Apply forward
recv._merg._process._store            # Chain multiple actions
```

---

## TIMELINE OPERATIONS

### Timeline Syntax
```blaze
timeline-[target]            # Name timeline
^timeline.[target]/          # Jump to timeline
```

### Timeline Bouncing
```blaze
^timeline.[target bnc unwanted recv]/
```

### Inline Jump-and-Resume Syntax
```blaze
^timeline.[marker|{do/x/}]/   # Rewind to "marker" then resume at next do/x/
^timeline.[safe|{do/continue/}]/
^timeline.[backup|{do/process/}]/
```

### Timeline Keywords
```blaze
timeline-        # Timeline definition
timeline.        # Timeline jump (with ^)
bnc             # Bounce
recv            # Receive
```

### Timeline Examples
```blaze
timeline-[|function|.safe_state]
^timeline.[|function|.safe_state]/
^timeline.[target bnc backup recv]/
^timeline.[checkpoint|{do/recover/}]/
```

---

## TEMPORAL LITERALS

### Compile-Time Temporal References
```blaze
@+2h         # Two hours in the future (compile-time)
@-3d         # Three days ago
@+15m        # 15 minutes future
@-1w         # One week ago
```

### Runtime Temporal References
```blaze
@!+2h        # Two hours from runtime start
@!-30m       # 30 minutes before runtime start
@!+1d        # One day after runtime start
```

### Temporal Units
```blaze
s            # Seconds
m            # Minutes
h            # Hours
d            # Days
w            # Weeks
```

---

## MATRIX SYSTEM

### Matrix Declaration
```blaze
[:::matrix_definition]
```

### Matrix Syntax
```blaze
[:::row-columns-scope[values]]
[:::student1-math-science[85;92;78]]
[:::data-metrics-timeframe[25%]]
```

### Matrix Splitting
```blaze
c.split._[description_count]
Crack._[description_count]
cac._[description_count]         # Shorthand for Crack._
```

### Split Examples
```blaze
c.split._[data_4]           # Split into fourths
c.split._[data_null]        # No splitting
c.split._[data_all]         # Maximum splitting
cac._[data_4]               # Shorthand version
```

---

## 4D ARRAYS

### Array Declaration
```blaze
|A| array.4d[x;y;z;t]<      # Declare a 4D array
```

### Temporal Access
```blaze
A[x;y;z;<t]/                # Read past time slice
A[x;y;z;>t]/                # Write future time slice
A[x;y;z;=t]/                # Current time slice
A[x;y;z;start:end]/         # Time range
```

### Array Examples
```blaze
|data| array.4d[10;5;3;12]<
data[1;2;3;<5]/             # Access past time t=5
data[1;2;3;>8]/             # Write to future time t=8
data[0;0;0;1:10]/           # Access time range 1-10
```

---

## OUTPUT METHODS

### Output Types
```blaze
print/           # Clean output (filter brackets)
txt/             # Literal output (show all)
out/             # Executable output (run commands)
fmt/             # Formatted output (apply styling)
dyn/             # Dynamic/conditional output
```

### Output Chaining
```blaze
out._fmt/        # Chain out/ then fmt/
txt._print/      # Chain txt/ then print/
out._dyn._fmt/   # Three method chain
```

### Output Examples
```blaze
print/[debug] Hello\               # Prints: Hello
txt/[debug] Hello\                 # Prints: [debug] Hello
out/[timestamp] Started\           # Prints: [2024-12-19] Started
fmt/[bold] Important\              # Prints: **Important**
out._fmt/[[bold] timestamp] Done\  # Chain methods
```

---

## ERROR HANDLING

### Error Handler Chains
```blaze
!-1             # Error handler 1 (highest priority)
!-2             # Error handler 2
!-N             # Error handler N (lowest priority)
```

### Error Syntax
```blaze
!-{number} func.err/{@param:type}< do/ handler\
```

### Error Examples
```blaze
!-1 error.catch/{@param:timeout}< do/ retry\
!-2 error.catch/{@param:overflow}< do/ cleanup\
!-3 error.catch/{@param:corrupt}< do/ restore|{do/backup/}\
```

---

## JUMP SYSTEM

### Jump Markers
```blaze
^marker_name     # Create jump point
^marker_name/    # Jump to point
```

### Jump Examples
```blaze
^safe_point
do/ risky_code/ ^safe_point/
```

---

## GAP ANALYSIS

### GAP Keywords
```blaze
gap.compute      # GAP computation
missing[data]    # Missing data specification
confidence       # Confidence assessment
```

### GAP Examples
```blaze
gap.compute/{@param:variable}/{@param:missing[items]}< do/
    analysis/ confidence_check\
```

---

## GGGX OPERATIONS

### GGGX Phase Keywords
```blaze
gggx.go          # GO phase (initialize)
gggx.get         # GET phase (acquire data)
gggx.gap         # GAP phase (analyze missing)
gggx.guess       # GUESS phase (interpolate)
gggx.execute     # eXecute phase (run if viable)
```

### GGGX Complete Flow
```blaze
gggx.go/{@param:task}< :>
gggx.get/{@param:metrics}< :>
gggx.gap/{@param:missing}< :>
gggx.guess/{@param:confidence}< :>
gggx.execute/{@param:threshold}< :>
```

---

## SEPARATORS

### List Separators
```blaze
;               # Semicolon for ALL lists
```

### Separator Usage
```blaze
[85;92;78]                  # Numbers
[cpu;memory;disk]           # Identifiers
[normal;stress;overload]    # Categories
```

**NEVER use commas (,) - always semicolons (;)**

---

## FUNCTION CONTROL

### Function-Body Early Return
```blaze
stop/           # Immediate return from current block or function
```

### Function Control Examples
```blaze
|check| verify.can/{@param:value}< :>
    do/
        f.if/{@param:value} *<0>
            stop/                    # Early return
        \>|
        process/{@param:value}/
    \
```

### Function Closers
```blaze
:>              # Function definition closer
```

### Closer Examples
```blaze
|func| method.can/{@param:x}< :>
|calc| math.can/{@param:a}/{@param:b}< :>
```

---

## COMMENTS

### Comment Syntax
```blaze
##              # Line comment
## text ##      # Inline comment
```

### Comment Examples
```blaze
## This is a comment
do/ code/ ## inline ## more_code\
```

---

## MEMORY ZONES

### Zone Keywords
```blaze
past_zone       # Past execution state
present_zone    # Current execution state
future_zone     # Future execution state
unknown_zone    # Uncertain data state
```

---

## SPECIAL OPERATORS

### Function Execution Control
```blaze
.can            # Conditional capability
.do             # Action execution
.compute        # Computation
.analyze        # Analysis
```

### Data Type Keywords
```blaze
var.v           # Variable declaration
array.4d        # 4D array type
matrix          # Matrix type
timeline        # Timeline type
blueprint.b     # Blueprint declaration
object.o        # Object instantiation
method.m        # Method declaration
```

---

## KEYWORDS AND RESERVED WORDS

### Core Keywords
```blaze
var             # Variable
do              # Action start
if              # Conditional
while           # Loop
until           # Loop until
try             # Attempt
can             # Capability
timeline        # Timeline operations
gap             # GAP analysis
gggx            # GGGX algorithm
array           # Array type
matrix          # Matrix type
blueprint       # Class definition
object          # Instance creation
method          # Method definition
stop            # Early return
```

### Method Keywords
```blaze
compute         # Computation
analyze         # Analysis
process         # Processing
create          # Creation
split           # Splitting
merge           # Merging
queue           # Queueing
bounce          # Bouncing
```

---

## TEMPORAL KEYWORDS

### Time-related
```blaze
past            # Past reference
present         # Present reference
future          # Future reference
unknown         # Unknown reference
temporal        # Temporal operation
```

---

## LOGICAL OPERATORS

### Boolean Logic
```blaze
and             # Logical AND
or              # Logical OR
not             # Logical NOT
true            # Boolean true
false           # Boolean false
```

---

## BUILT-IN FUNCTIONS

### Math Operations
```blaze
+               # Addition
-               # Subtraction
*               # Multiplication
/               # Division
%               # Modulo
```

### Comparison Base
```blaze
*               # Comparison prefix
=               # Equality base
!               # Negation base
_               # Equal modifier
```

---

## FILE EXTENSIONS

### Blaze Files
```blaze
.blaze          # Blaze source files
.bz             # Abbreviated extension
```

---

## OPERATOR PRECEDENCE (Highest to Lowest)

1. **Pipe identifiers**: `|name|`
2. **Parameters**: `{@param:name}`
3. **Brackets**: `[]`, `{}`
4. **Temporal literals**: `@+2h`, `@!-30m`
5. **Unary operators**: `!`, `-`, `*`
6. **Multiplicative**: `*`, `/`, `%`
7. **Additive**: `+`, `-`
8. **Comparison**: `*>`, `*<`, `*_>`, `*_<`
9. **Equality**: `*=`, `*!=`
10. **Logical AND**: `and`
11. **Logical OR**: `or`
12. **Timing**: `<`, `>`, `<<`, `>>`, `<>`, `>/>`
13. **Time-rate**: `>/>`, `>\>`, `</<`, `<\<`, `||`
14. **Connectors**: `._`, `\>|`, `\<|`
15. **Assignment**: `=`

---

## SYNTAX VALIDATION RULES

### Required Elements
- All parameters must use `/{@param:name}` format
- All lists must use semicolon separators `;`
- Function names must be in pipes `|name|`
- Action blocks must start with `do/` and end with `\`
- Timeline operations require `^` for jumping
- Function headers must end with `< :>`
- Methods must use `method.m-name` format

### Forbidden Elements
- Parentheses `()` - not used in Blaze
- Commas `,` in lists - use semicolons `;`
- Braces `{}` except for parameters
- C-style comments `/* */` - use `##`

---

## COMPLETE EXAMPLE

```blaze
## Complete Blaze program showing all features

blueprint.b-timekeeper-[name;startTime;events]

|main| entry.can< :>
    do/
        ## Create timeline marker
        timeline-[program_start]
        
        ## Instantiate object
        object.o-keeper-[timekeeper]
        
        ## Set temporal literal
        var.v-deadline-[@!+2h]
        
        ## Fast-forward test
        >/> (10)
        
        ## Conditional with comparison
        f.ens/{@param:deadline} *>@+1h>
            print/"Still have time"\
        \>|
        
        ## 4D array access
        |data| array.4d[10;10;10;24]<
        data[5;5;5;<12]/
        
        ## GGGX flow
        gggx.go/{@param:analysis}< :>
        gggx.gap/{@param:missing[cpu;memory]}< :>
        
        ## Error chain
        !-1 error.catch/{@param:timeout}< do/ 
            ^timeline.[program_start|{do/restart/}]/
        \
        
        ## Output chaining
        out._fmt/[bold] "Complete"\
        
        ## Early return
        f.if/{@param:done} *=true>
            stop/
        \>|
        
        ## Matrix splitting
        cac._[data_4]
        
        ## Pause and resume
        ||
        <\< (2)
    \
```

---

This is the COMPLETE syntax reference for implementing the Blaze language lexer, parser, and compiler.