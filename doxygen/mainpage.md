# %QtPass developer documentation {#mainpage}

%QtPass is a multi-platform GUI for [pass](https://www.passwordstore.org/), the
standard Unix password manager. This site documents its source code for people
who work on it. %QtPass has no public API: everything here, private members
included, is described for contributors.

If you want to use %QtPass rather than change it, <https://qtpass.org/> and the
[Readme](https://github.com/IJHack/QtPass#readme) are the places to go.

## Where to start

Read these four classes first; almost everything else hangs off them.

1. Pass is the abstract password store: show, insert, remove, move, copy,
   init, Git, key generation and search. Its class description states the
   rules every operation keeps about paths and links.
2. Executor runs the external programs. Every `gpg`, `git` and `pass` process
   goes through its queue, one at a time and in order.
3. MainWindow owns the widgets and turns user actions into calls on Pass.
4. QtPassSettings holds the configuration, and hands out the active backend.

The @ref backend "topics" below group the remaining classes by what they are
for.

## Two backends

%QtPass can work a password store in two ways, chosen by the "use pass"
setting.

- RealPass hands every operation to the `pass` command-line tool.
- ImitatePass does what `pass` does by running `gpg` and `git` itself, for
  systems where `pass` is not available (Windows, most of all).

Both derive from Pass, so the rest of the program does not know which one is
active. PassBackendFactory constructs them on first use and picks one;
QtPassSettings::getPass() is how callers reach it.

An operation in ImitatePass is usually several processes: an insert is `gpg`,
then `git add`, then `git commit`. SimpleTransaction groups them so the
operation is reported once, when its last process ends.

## How an operation runs

Nothing in the interface waits for a process. A call on Pass queues work and
returns; the result arrives later as a signal.

<div class="interactive_dotgraph">
@dot
digraph flow {
rankdir=LR;
node [shape=box, fontname=Helvetica, fontsize=12, margin="0.15,0.1"];
edge [fontname=Helvetica, fontsize=11];
ui [label="MainWindow\nor a dialog", URL="@ref MainWindow"];
pass [label="Pass\n(RealPass / ImitatePass)", URL="@ref Pass"];
exec [label="Executor\nFIFO queue", URL="@ref Executor"];
proc [label="child process", shape=ellipse];
ui -> pass [label="Show(), Insert(), ..."];
pass -> exec [label="execute()"];
exec -> proc [label="QProcess"];
proc -> exec [label="exit"];
exec -> pass [label="finished() / error()"];
pass -> ui [label="finishedShow(), ...\nprocessErrorExit()"];
}
@enddot
</div>

Showing an entry, step by step:

1. MainWindow calls Pass::Show() on the active backend.
2. The backend refuses the path if it leads through a link, and otherwise
   queues one process on its Executor: `pass show` in RealPass, `gpg -d` in
   ImitatePass.
3. Executor starts the process when everything queued before it has ended,
   and emits Executor::finished() or Executor::error() when it exits.
4. Pass::finished() receives both. Exit code 0 becomes the signal of that
   kind of operation, here Pass::finishedShow(); anything else becomes
   Pass::processErrorExit().
5. MainWindow, the QtPass glue object and the open dialogs are connected to those signals and
   update what the user sees.

An operation that queued a command ends in a `finished...` signal or in
Pass::processErrorExit(), and code that disables part of the interface while
it runs relies on that to be released again. Checks made before anything is
queued are less uniform: some refusals emit Pass::processErrorExit() too,
others only Pass::critical(). The signal descriptions in Pass say which.

A few things do not go through the queue. Executor::executeBlocking() runs a
process and waits; it is for worker threads and for short probes. NativeGrep,
the content search, decrypts on a thread of its own and reports through
Pass::finishedGrep(). Re-encrypting a folder in ImitatePass runs on a worker
thread as well.

## Rules about secrets

Decrypted entries pass through this code, and several rules keep them from
ending up where they should not. Please keep them when changing things.

- Every kind of process is classified in one table, Enums::processInfo(), as
  able to output a secret or not. Output that may hold one reaches neither the
  process output panel nor Pass::finishedAnyWithPid(). A new Enums::PROCESS
  value does not compile until it has a row there.
- ClipboardManager is the only way a secret reaches the clipboard. It marks
  the data so clipboard managers skip it, remembers what it copied, and clears
  it again.
- SecretLineEdit, SecretPlainTextEdit and SecretTextBrowser are the widgets
  that may hold a secret. Whatever is copied out of them, the X11 primary
  selection included, is marked as a secret and cleared like any other copy.
- Nothing behind a link is treated as part of the store. Pass::refuseLinkedPath()
  and PathValidator hold the checks.

## Topics

- @ref backend
- @ref process
- @ref keys
- @ref store
- @ref settings
- @ref ui
- @ref secrets
- @ref support

## Working on these docs

The pages are generated by Doxygen from the comments in `src/` and `main/`,
from this page and from `doxygen/groups.dox`. The documentation job fails on
any Doxygen warning, so run it before pushing:

```bash
doxygen Doxyfile >/dev/null
```

Nothing printed means no warnings. Open `docs/index.html` to read the result.

What a comment needs:

- Every class, function, signal, slot, member variable and enum value has a
  doc block, private ones as well. Non-void functions have `@return`, and
  every parameter has `@param`.
- The block in the header is the contract: what the thing does, when a signal
  is emitted and what the receiver should do, what a member holds and who owns
  it. Say what the name does not already say.
- For a function declared in a header, a block on the definition in the
  `.cpp` file is for how it works. Write it as plain paragraphs without
  `@brief`, `@param` or `@return`; Doxygen appends it to the description from
  the header.
- A function that exists only in a `.cpp` file, such as a `static` helper, has
  no header block. Its full block, with `@brief`, `@param` and `@return`, goes
  on the definition.
- Each file starts with a `@file` block, and each class belongs to one topic
  through `@ingroup`.

Building, testing and the pull request workflow are described in
[CONTRIBUTING.md](https://github.com/IJHack/QtPass/blob/main/CONTRIBUTING.md).
