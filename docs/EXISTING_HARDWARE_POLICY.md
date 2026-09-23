# Existing-hardware policy

The first stable AlfaRaceX release targets the installed board exactly as it is.
No MCU replacement, PCB modification, wiring change or external add-on is part of the
1.0.0 acceptance criteria.

## Application memory budgets

The target linker budgets intentionally match the currently deployed firmware layout:

- C1 application: 96 KiB from `0x08000000`
- C2 application: 60 KiB from `0x08000000`
- BH application: 60 KiB from `0x08000000`

The persistent pages remain at the same high addresses already used by the installed
firmware:

- visible parameters: `0x0801E800`
- statistics: `0x0801F000`
- settings: `0x0801F800`

AlfaRaceX will not require a different MCU to reach stable status. The cross-build
must fit these existing application budgets and preserve the established persistent
layout.

## Release rule

A feature that needs a hardware modification is outside the 1.0.0 replacement scope.
Such work can only be introduced later as an optional hardware profile.
