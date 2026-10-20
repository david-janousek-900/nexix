# Roadmap coverage and status contract (task 37)

[tasks.md](tasks.md) is the authoritative numbered 0.1.0 roadmap.
[roadmap-coverage.json](roadmap-coverage.json) maps every inventory component
and platform/supporting requirement to its owning or supporting task IDs.
The [inventory](v7-userland-manifest.json) points to that map through
`roadmap_coverage`; the inventory itself retains schema version 2.

## Coverage format

Coverage schema version 2 contains the release version, task-list/inventory paths,
task count, policy and two mappings: `components` and `requirements`. Mapping
keys are inventory IDs; values are nonempty, sorted, unique arrays of task IDs.
All 217 components, all 36 requirements and all 386 tasks must be accounted for.
There is no name-based inference or fallback assigning omitted work to a generic
release task. New components and requirements require explicit mappings.

One component can span several tasks, and several components may share a task.
For example, init has its initial native port and its later multi-user work;
diff3's compiled helper and installed script share one integration task. Native
kernel functionality spans existing foundations and remaining adaptations.
Mappings establish responsibility, not a claim that each mapped task implements
every source belonging to that component. Task 38 records kernel source
responsibility; field/symbol contracts remain tasks 39–40 and 82, with kernel
behavior closure assigned to the individual feature tasks.

Build/runtime/optional-runtime dependency edges, evidence and certainty remain
in the inventory. Follow those edges through the coverage map when identifying
prerequisite work. A mapping does not require completion of every future task
for an already working subset, and does not invent an execution order for
generator/toolchain bootstrap cycles. Resolve remaining source-specific
dependency audits during the corresponding implementation tasks.

## Separate status meanings

Roadmap tasks use `Done`, `In progress` and `To do`. Done requires implementation,
acceptance tests and a commit. Existing rows pin historical commit hashes.
For the commit creating or completing its own row, the notes point to Git
history: a commit cannot include its own final hash. Record actual test evidence
and use the Git log for that row's commit instead of a placeholder hash.

Inventory components use `implemented`, `partial`, `not_ported`, `prototype`
and `pending_assessment`. A completed prerequisite cannot make an unfinished
component complete. Implemented components need at least one completed mapped
task; unfinished components must retain at least one unfinished mapped task.
An implemented initial program can also have future integration work, as init
does. Supplied requirement inputs, generated recipes and preserved historical
binaries do not establish native functionality.

The checker validates recorded claims and structural coverage; it does not
re-execute every historical test or cryptographically prove a commit's contents.
Do not mark a task done based only on a passing inventory check.

## Validation and maintenance

```sh
make TOOLCHAIN_PREFIX= test-inventory
```

The read-only checker validates archive fidelity and inventory consistency,
then rejects missing/unknown component or requirement mappings, invalid or
duplicate references, unowned tasks, malformed task-list rows, missing/reordered
IDs, status-total drift and misleading completion mappings. Negative regression
tests exercise these failures without editing the real task list or inventory.

When completing a task, update its task-list status, notes and summary totals,
record tests and commit the change with its implementation. When adding work,
append a stable sequential ID, update `task_count`, task-list totals and explicit
coverage mappings. Preserve existing IDs. Component implementation statuses
change only when their own output/consumer acceptance is satisfied.

The previous 29-task roadmap is mapped in [tasks.md](tasks.md). Detailed documents now
use expanded IDs where milestones above 24 changed. Old Git commit descriptions
retain their historical numbering. No reference sources or current native
program implementation claims are changed by task 37.

Task 38 adds the [kernel source responsibility audit](kernel-coverage.md),
including 113 reference inputs and 64 original syscall slots. Newly discovered
kernel prerequisites retain stable IDs 362–364 and precede final release gates.

Task 39 records all 57 original installed-header inputs in
[header-coverage.json](header-coverage.json). Native contracts and consumer-owned
formats are distinct; adding a declaration does not complete its feature owner.

Schema version 2 renames `step_count` to `task_count`. Kernel and header
coverage also use schema version 2, with `tasks` and `task` ownership fields.
Consumers must migrate these field names; numeric IDs retain their meaning.
