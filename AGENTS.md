# GOJI engineering instructions

These instructions apply throughout this repository. Make the smallest coherent change that satisfies the user's request while preserving existing customer workflows. This file records engineering policy; it is not authorization for a general cleanup or architecture migration.

## Preserve behavior and control scope

- Preserve legacy, compatibility, migration, fallback, historical-path, and recovery behavior unless the user explicitly directs otherwise. Apparent age, duplication, lack of callers, or a deprecation comment alone does not authorize removal.
- Treat unusual customer-specific behavior as intentional unless the requested task explicitly changes it. If source, tests, and operator instructions disagree, identify the conflict; do not silently choose a more conventional business rule.
- When intent cannot be established, preserve the behavior and report the uncertainty. Ask a focused question only when that uncertainty blocks the requested change.
- RAC is currently inactive, but must remain available as dormant functionality that may need reactivation. Do not delete, redesign, normalize, or reactivate RAC solely because it is unused today. Preserve its related code, configuration, resources, and local ignored scripts.
- Do not perform unrelated refactoring unless explicitly requested. During another task, allow only tiny simplifications directly necessary to that task. Report other cleanup opportunities separately.
- Preserve existing user changes, untracked tests, ignored scripts, and local artifacts. Git ignore rules do not establish that a file is disposable. Do not stage unrelated files.

## Understand the affected workflow first

Before editing, inspect repository status and trace the affected path through its callers, UI wiring, controller, persistence, filesystem operations, Python stages, and result consumers as applicable. Read relevant tests, comments, embedded operator instructions, and history when intent is unclear. Search for an existing implementation before adding another.

The architectural baseline is the repository survey at commit `058e3e9`; verify relevant details against the current checkout rather than treating the survey as permanently current:

- GOJI is a Windows Qt/C++17 desktop application built with qmake through `GOJI.pro`. Most C++ sources and headers are at the repository root; `GOJI.ui` defines the main form.
- `MainWindow` wires the application and workflows. Many modules have controller, database-manager, and file-manager families. `BaseTrackerController` shares clipboard behavior; it is not a universal workflow engine.
- AILI and Weekly PIDO differ from the common tracker pattern. Preserve these differences; the survey does not establish their full design rationale. Do not standardize them merely to resemble other modules.
- SQLite connection management is shared, but schemas, job identities, update semantics, and migrations differ by workflow. Controllers also use SQL models directly.
- `scripts/` contains customer pipelines and standalone tools, not a single packaged Python application. Numbered scripts often represent operator-mediated stages.
- `resources/` contains operator HTML and styling; `resources.qrc` embeds resources. Keep affected instructions consistent with authorized workflow changes.

## Keep implementation complexity proportional

- Correct the underlying implementation within the authorized scope instead of adding another workaround, fallback, flag, or parallel source of truth around it.
- Prefer clear local logic and existing appropriate helpers. Add an abstraction only when it serves a concrete need and reduces total complexity. Explain any new layer or dependency.
- Similar code does not establish identical business semantics. Confirm equivalence before sharing behavior across customers. Do not consolidate path resolvers, CSV readers, ID generators, archive routines, clipboard mechanisms, or Healthy/Broken structures solely because they look alike.
- Keep customer rules in their appropriate workflow. Do not introduce customer-specific branches into shared infrastructure without a demonstrated need.
- When explicitly replacing functionality, remove only the superseded implementation within scope after checking callers and contracts. This does not override the requirement to preserve compatibility and dormant functionality.
- Match nearby naming, formatting, and error-reporting conventions. Avoid broad formatting changes, speculative extensibility, and new frameworks for a local fix.

## Qt, UI, and process lifecycle

- Preserve widget object names, signal/slot contracts, initialization order, reset/load behavior, job/postage locks, and instruction-state transitions unless the task changes them. These locks are workflow state, not automatically database concurrency locks.
- Follow the affected module's ownership model. Verify QObject parenting separately from ordinary C++ ownership, and account for asynchronous callbacks, dialogs, timers, watchers, and process lifetime.
- Keep long-running work from introducing new UI blocking. Use the existing asynchronous process pattern where appropriate and handle success, failure, cancellation, and stale job context.
- Edit `GOJI.ui`, source files, and resource manifests rather than generated files. Do not create or regenerate a root-level `ui_GOJI.h`; qmake places generated UI headers under its configured build directory.
- Update `GOJI.pro` and `resources.qrc` when source/resource additions require it. Use the configured Qt/MinGW toolchain; inspect local configuration before building. `Clean Build.bat` deletes artifacts and is not a routine prerequisite for validation.

## Persistence, identity, and file safety

- Preserve each workflow's complete job identity, uniqueness rules, NULL/empty-text semantics, and state-preservation behavior. Do not interchange SQL upsert strategies without examining their effects.
- Use prepared queries and bound values for new SQL. Keep schema changes compatible with existing installations and validate migrations on disposable database copies, including existing data.
- Trace identity edits across database rows, tracker models, directories, archives, manifests, and recovery markers. A SQLite transaction does not make filesystem or network operations atomic.
- Preserve original inputs, collision handling, archive boundaries, rollback behavior, and rerun/recovery guarantees. Do not remove recovery markers or clean working data merely to make a retry succeed.
- Validate outputs before replacing valid existing results or deleting inputs. For changes affecting failure handling, exercise partial-failure and retry behavior in an isolated fixture.
- Respect workflow-specific canonical, configured, legacy, NAS, Desktop, and Downloads paths. Do not normalize settings stores or fallback destinations as incidental cleanup.

## Python stages and cross-language contracts

- Treat script paths/names, arguments, working directories, exit codes, stdout markers, JSON schemas, generated filenames, column schemas, manifests, and recovery markers as interfaces. Inspect both producer and consumer before changing one.
- Exact markers such as `TMMA_FINAL_OUTPUT_FILE=`, `TMMA_FINAL_MERGED_FILE=`, and `TMCA_RESULT_BEGIN`/`TMCA_RESULT_END` are not cosmetic logging. Keep diagnostic output from corrupting structured results.
- Preserve human handoffs through Bulk Mailer, Office, email dialogs, and other production tools. A successful process exit may mean a phase ended rather than the entire workflow completed. `PAUSE_FOR_EMAIL` may indicate exit followed by a separate archive invocation, not a process waiting for stdin.
- Preserve stage boundaries and established preparation/archive modes. Do not automatically advance past an operator decision or move logic between stages without tracing its consequences.
- Treat ZIP codes, account numbers, MATCHIDs, and similar identifiers as strings unless the workflow explicitly requires numeric semantics. Preserve leading zeros, blanks, encoding, delimiters, headers, ordering, and matching-ID stability as required by that workflow. Prevent implicit pandas type/NA conversion from changing business data.
- Do not generalize customer-specific filtering, duplicates, address handling, class displays, or version classification across workflows without explicit scope and evidence.
- Inspect scripts before importing or executing them: some perform work at import time, prompt for input, or use production paths by default.

## Required runtime script synchronization

Repository scripts and deployed scripts are separate files. Whenever a repository Python script is modified, also update its corresponding runtime copy under `C:\Goji\scripts` as part of the same task. This is standing user authorization; routine synchronization does not need a fresh permission request.

1. Identify the exact repository-to-runtime mapping. Normally `scripts/<relative path>` maps to `C:\Goji\scripts\<relative path>`; verify actual launch paths when the mapping is uncertain.
2. Inspect the destination before replacement. If it contains unexplained runtime-only changes, preserve a recoverable copy and reconcile them; do not silently discard changes or copy an entire script tree over production.
3. Validate the intended script changes using isolated inputs, then copy the finalized affected files. Include any changed companion files required for the deployed script to work. Do not interrupt an active workflow or leave an incompatible set of scripts in use.
4. Verify source and runtime copies match using hashes or a byte comparison. If a script changes again after validation or review, synchronize and verify it again.
5. Report the runtime destinations and verification result. If the path is unavailable, a conflict cannot be resolved, or a running workflow prevents safe replacement, report the exact blocker and that synchronization remains incomplete. Do not claim full completion.

Project inclusion through qmake does not prove deployment. This synchronization rule does not itself authorize running production workflows, modifying live business data, publishing a release, or replacing application binaries.

## Validation and honest coverage reporting

- Run relevant existing automated tests after the final implementation. For behavioral changes, add a focused regression test when practical, covering the changed behavior rather than merely mirroring the implementation.
- Use synthetic fixtures, temporary directories, disposable database copies, and supported path overrides. Inspect tests for side effects before running them. Do not use live customer data, production archives, or network deliveries as casual test fixtures.
- Known Python suites are listed below. The survey found TM MA tracked and Four Hands/Combine Data untracked; check current status and dependencies. Do not delete or silently stage local tests.

```powershell
python -B -m unittest discover -s "scripts/TRACHMAR/MA/tests" -p "test_*.py" -v
python -B -m unittest discover -s "scripts/FOUR HANDS/tests" -p "test_*.py" -v
python -B -m unittest discover -s "scripts/tests" -p "test_*.py" -v
```

Run these from the repository root with the appropriate Python environment. Select suites relevant to the change; these commands are not a guarantee that dependencies are installed or tests pass.

- The survey found no C++ test target or CI workflow. Verify current availability. For C++/UI changes, build the affected application when possible and perform focused interaction checks for the changed behavior.
- If no automated test exists for the affected behavior, explicitly say so and perform reasonable alternative validation: syntax checks, producer/consumer inspection, fixture-based execution, output comparisons, a build, or a targeted manual check as appropriate.
- Distinguish passed checks, failed checks, unavailable checks, and checks not run. Syntax checks are not behavioral tests; source-text assertions are not compiled C++ or UI tests; a successful build is not end-to-end workflow validation.
- For documentation-only changes, review content, paths, policy consistency, and the final diff; an application build is unnecessary unless executable behavior is affected.

## Mandatory completion review

Before declaring completion:

1. Read the entire task diff, including newly created files, and confirm each change is necessary. Preserve pre-existing work.
2. Review for duplicate logic, unnecessary helpers or abstractions, redundant state, excessive branching, and workarounds that obscure the actual fix. Simplify within scope while preserving compatibility and customer behavior.
3. Recheck affected contracts and failure/recovery paths. Rerun relevant validation if the review changes the implementation.
4. For modified Python scripts, confirm runtime synchronization and content equality after the final edit.
5. Report what changed and why, validation actually performed, missing test coverage or remaining uncertainty, runtime synchronization when applicable, and any necessary complexity introduced.

Do not claim a build, test, deployment, or workflow succeeded without observing its result. Keep unrelated complexity findings separate from the completed change.
