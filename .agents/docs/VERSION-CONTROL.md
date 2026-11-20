
# Version Control System
This repository uses git as its version control system.

## General Behavior

1. Always ask whether to work in a new git worktree
2. Independent from the worktree, always create a new branch according to the git-flow principles:
  - Feature development belongs in a feature/... branch
  - Bugfixes belong in a bugfix/... branch
  - Hotfixes belong in a hotfix/... branch
3. Commit logical steps and changes into the dedicated branch
4. never merge created or existing branches without explicit user consent. you always have to ask before merging.
5. provide excellent commit messages. they should follow the rules below under "Commit Message Rules and Guidelines"
6. Never, under no circumstances push any code to remotes. Fetching from remotes is explicitly allowed and encouraged, pulling requires explicit user consent.

## Semantic Versioning 2.0.0

Given a version number MAJOR.MINOR.PATCH, increment the:

1. **MAJOR** version when you make incompatible API changes
2. **MINOR** version when you add functionality in a backward compatible manner
3. **PATCH** version when you make backward compatible bug fixes

Additional labels for pre-release and build metadata are available as extensions to the MAJOR.MINOR.PATCH format.

## Commit Message Rules and Guidelines
### Structure
The use of a structured commit message is mandatory. A simple commit message is usually composed of a single sentence (= the Subject). However, larger or more complex commits may require additional information. In this case a **body** and **footers** can be added. This convention dovetails with aforementioned Semantic Versioning, by describing the features, fixes, and breaking changes made in commit messages.

The structure is as follows:
```
<Subject>

<optional body>

<optional footer(s)>
```
The first line of the message is the **subject**, intended to describe the commit briefly. Thus, this serves as a title for the commit. Therefore, it should be clear and concise. This part must be included in all messages. Details of how to write a subject is provided under "Conventional Commits".

The **body** provides more detailed information about the changes made. Usually, you need more than one sentence to write a good body.

A **Footer** can be added to the commit message. It can contain tags or external references. In this case, the tags indicate the type of change (feature, bug fix, etc.), and the external references (such as ticket numbers) specify which issue is being addressed. If there is no documented or referenced external reference, don't make one up. In that case, omit the references.

Each of these parts of the message are separated by a blank line.

### Content of the message
#### Subject

The **subject**** of the message should be structured as follows:
`<type>[optional scope]: <description>`


#### Types:

1. **fix**: a commit of the *type* `fix` patches a bug in your codebase (this correlates with `PATCH` in Semantic Versioning).
2. **feat**: a commit of the *type* `feat` introduces a new feature to the codebase (this correlates with `MINOR` in Semantic Versioning).
3. *types* other than `fix:` and `feat:` are allowed, for example `build:`, `chore:`, `ci:`, `docs:`, `style:`, `refactor:`, `perf:`, `test:`, and others.

Additional types are not mandated by the Conventional Commits specification, and have no implicit effect in Semantic Versioning (unless they include a BREAKING CHANGE).


The most essential principle when writing the **subject** is to use the imperative mood. Therefore, instead of saying “Adding…”, “Fixing…”, “Reverting…”, etc., use “Add…”, “Fix…”, and “Revert…”, respectively.

Another good practice is to emphasize the *"why"* and not just the *"how"*. For example, instead of writing “Add import_key() function” in the subject, it would be better to write something like “Add support for users to import keys”.


**BREAKING CHANGE:**
A commit that has a footer `BREAKING CHANGE:`, or appends a `!` after the type/scope, introduces a breaking API change (correlating with `MAJOR` in Semantic Versioning). A BREAKING CHANGE can be part of commits of any *type*.


**Scope:**
A scope may be provided to a commit’s type, to provide additional contextual information and is contained within parenthesis, e.g., `feat(parser): add ability to parse arrays`.

#### Body

This idea extends to the **body** of the commit. In particular, you should write the body so that other contributors can understand the changes and why. As an example, a good body for the subject presented in the previous paragraph would be something like this:

`Currently, users can only use keys created within the platform. Adds the import_key() function to allow the import of keys created externally.`


#### Footers

Footers other than BREAKING CHANGE: <description> may be provided and follow a convention similar to git trailer format.

### Strict rules for commit messages
1. Commits MUST be prefixed with a type, which consists of a noun, `feat`, `fix`, etc., followed by the OPTIONAL scope, OPTIONAL `!`, and REQUIRED terminal colon and space.
2. The type `feat` MUST be used when a commit adds a new feature to your application or library.
3. The type `fix` MUST be used when a commit represents a bug fix for the application.
4. A scope MAY be provided after a type. A scope MUST consist of a noun describing a section of the codebase surrounded by parenthesis, e.g., `fix(parser):`. In monorepos, scopes MUST be added at least to specify the subrepo the commit targets.
5. A description MUST immediately follow the colon and space after the type/scope prefix. The description is a short summary of the code changes, e.g., *fix: array parsing issue when multiple spaces were contained in string.*
6. A longer commit body MAY be provided after the short description, providing additional contextual information about the code changes. The body MUST begin one blank line after the description.
7. A commit body is free-form and MAY consist of any number of newline separated paragraphs.
8. One or more footers MAY be provided one blank line after the body. Each footer MUST consist of a word token, followed by either a `:<space>` or `<space>#` separator, followed by a string value (this is inspired by the git trailer convention).
9. A footer’s token MUST use - in place of whitespace characters, e.g., `Acked-by` (this helps differentiate the footer section from a multi-paragraph body). An exception is made for BREAKING CHANGE, which MAY also be used as a token.
10. A footer’s value MAY contain spaces and newlines, and parsing MUST terminate when the next valid footer token/separator pair is observed.
11. Breaking changes MUST be indicated in the type/scope prefix of a commit, or as an entry in the footer.
12. If included as a footer, a breaking change MUST consist of the uppercase text BREAKING CHANGE, followed by a colon, space, and description, e.g., *BREAKING CHANGE: environment variables now take precedence over config files.*
13. If included in the type/scope prefix, breaking changes MUST be indicated by a `!` immediately before the `:`. If `!` is used, `BREAKING CHANGE:` MAY be omitted from the footer section, and the commit description SHALL be used to describe the breaking change.
14. Types other than `feat` and `fix` MAY be used in your commit messages, e.g., *docs: update ref docs.*
15. The units of information that make up Conventional Commits MUST NOT be treated as case-sensitive by implementors, with the exception of BREAKING CHANGE which MUST be uppercase.
16. BREAKING-CHANGE MUST be synonymous with BREAKING CHANGE, when used as a token in a footer.



### Brevity 
For commit messages **use the 50/72 rule**. 
50 refers to the **maximum number of characters** of the commit message’s **subject** (or title). Meanwhile, 72 indicates that you should format the body of the message in columns of up to 72 characters.


### Examples of good commit messages
- Reverting commit
  To revert a commit use the revert type, and a footer that references the commit SHAs that are being reverted:

  ```
  revert: let us never again speak of the noodle incident

  Refs: 676104e, a215868
  ```
- Commit message with description and breaking change footer
  ```
  feat: allow provided config object to extend other configs

  BREAKING CHANGE: `extends` key in config file is now used for extending other config files
  ```
- Commit message with `!` to draw attention to breaking change
    ```
    feat!: send an email to the customer when a product is shipped
    ```
- Commit message with scope and ! to draw attention to breaking change
    ```
    feat(api)!: send an email to the customer when a product is shipped
    ```
- Commit message with both `!` and BREAKING CHANGE footer
  ```
  feat!: drop support for Node 6

  BREAKING CHANGE: use JavaScript features not available in Node 6.
  ```
- Commit message without body
  ```
  docs: correct spelling of CHANGELOG
  ```
- Commit message with scope
  ```
  feat(lang): add Polish language
  ```
- Commit message with multi-paragraph body and multiple footers
  ```
  fix: prevent racing of requests

  Introduce a request id and a reference to latest request. Dismiss
  incoming responses other than from latest request.

  Remove timeouts which were used to mitigate the racing issue but are
  obsolete now.

  Reviewed-by: Z
  Refs: #123
  ```
