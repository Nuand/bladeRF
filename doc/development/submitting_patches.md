bladeRF Patch Submission Guidelines
================================================================================

In order to ensure your patches are accepted and merged in a timely fashion,
please follow the guidlines presented in this document.

Do not be surprised or discouraged if you are asked to iteratively change or
improve portions of your patches before they are accepted. This is simply
intended to help improve quality and consistency.

In general, the easier you make it for the bladeRF devs to test and merge your
changes, the quicker the process will be.

For significant changes/fixes, please feel free to include an update to the
CONTRIBUTORS file, or provide the information you would like included in an
email to bladeRF@nuand.com.

If you have not already, please review the
[bladeRF Style and Coding Conventions](style_and_conventions.md).

--------------------------------------------------------------------------------


## Contributor Assignment Agreements ##

For significant contributions, it is required that you sign and submit a
Contributor Assignment Agreement (CAA) to bladeRF@nuand.com to assign copyright
to Nuand, LLC. This is intended to allow Nuand to:
 * Ensure it has the necessary ownership or grants of rights over all
   contributions to allow them to be distributed under the chosen licence(s).
 * Address any (re)licensing issues, should they arise, without needing to
   coordinate and confirm with every single contributor.

Please use the [Individual CAA](../../legal/bladerf_icaa.txt) if you are
representing yourself as an individual, or otherwise, the
[Entity CAA](../../legal/bladerf_ecaa.txt).

If have you have quesions or concerns, or if it is not possible for you to
submit a CAA, *do not let this dissuade you from contributing*. Contact us at
bladeRF@nuand.com to discuss your situation.

If you do not plan to submit patches using your real name, please ensure to
make note of the email address and/or pseudonym you will be using when
submitting the ICAA or ECAA.


## Guidelines ##

### Format ###

Generate patches using `git format-patch`. Please ensure your `.gitconfig`
contains appropriate values for the `name` and `email` fields.

### Commit messages ###

All lines in the commit message should be under 80 characters. Generally,
the first line should be 50-72 characters when emailing patches. The
80-character limit helps ensure the git log is easy to view in terminals,
various git tools, and on GitHub.

#### Summary line ####

The first line in your commit message is what will generally be shown in log
summaries, so it is important that it concisely and accurately summarizes your
change. If possible, note the associated issue #. It should be formatted as
follows:

`[module]: Brief summary of change goes here`

For example, if your change pertains to the bladeRF-cli:

`bladeRF-cli: Added an 'example' command to do such and such`

Or if you made a change to the FPGA:

`hdl: Registered output of <module> to address issue #972`


#### Detailed Description ####

In the remainder of the commit message, describe *why* the change is necessary,
and *what* is addressing.

Describe the underlying problem you are addressing, referencing the appropriate
issue tracker item or describing associated symptoms/observables.

If you are making improvements, optimizations, etc., try to include
quantifications of these to justify your change.

Provide a sufficiently detailed technical summary of your change(s).  Although
this is a rather subjective request, try to strive for a level of detail that
will be helpful when reviewing the change log months down the road, as this
information will be used when tracking down newly discovered defects.  In
general, your commit message should provide a sense of:

 * What portions of the code have been changed
 * How your changes affect device operation


### One objective per commit ###
Each commit should contain cohesive changes that focus on only **one**
objective.

For example, if you have fixed a defect and have added new functionality, these
changes should be submitted as two separate commits: one for
resolving the defect, and another for introducing the new feature.

The rationale is that it is easier to bisect the codebase to track down changes
(and any new defects associated with them) when commits are cohesive and of a
smaller scope.  Following the above example, if a new defect were introduced by
one of the two commits, it would be easier to determine which of
the two were responsible via `git bisect`.

When separating your commits, ensure that the codebase builds and runs with
each changeset. If this is not possible, you may group the changes.

This is most easily achieved by creating a branch for each task you work on,
and generating patches from that branch.

### Rebase against upstream master ###

Please rebase your patches against the upstream master. (See `git rebase`, and
the interactive ```-i``` option).

It is understood that master updates frequently and you may be a few
commits behind. However, having your changes apply cleanly to master will help
ensure your changes will get merged in a timely fashion.

### Squash commits ###

For the reasons outlined in the previous two sections, use `git rebase -i`, to
squash your commits to the smallest number of cohesive changesets.
If you are new to this feature, it is **highly** recommended that you perform
this operation in a staging branch, allowing you to easily get back to your
original set of changes, should you make a mistake.

While micro-committing is certainly helpful when developing, there is generally
no reason to introduce a history of partial implementations or fixes in
upstream repository. Just submit the final, complete, and fully-functional
changesets.

For example, consider the following set of changes.

<pre><code>
bladeRF-cli: Renamed command 'do_x' to 'do_y'                           (8)
libbladeRF: Updated X to Y in API header example snippet                (7)
libbladeRF: Renamed functionality X to Y                                (6)
libbladeRF: Fixed off-by-one bugs in functionality X                    (5)
libbladeRF: Added sample usage of X to the API header file              (4)
libbladeRF: Fixed typos in functionality X                              (3)
bladeRF-cli: Added command 'do_x' to support libbladeRF functionality X (2)
libbladeRF: Added functionality X                                       (1)
</code></pre>

It is undesirable to introduce the typo fixes and renaming commits into the
upstream codebase when a more final and complete set of changes can instead be
merged, yielding a more straight-forward change history.

In this case, squash and reorder the commits prior to submitting them:

 * Squash `(1)`, `(3)`, `(5)` and `(6)` into one commit `(a)`. These libbladeRF
   changes will be the first patch/commit.
 * Squash `(4)` and `(7)` into `(b)`. This will be the second commit.
 * Squash `(2)` and `(8)` into `(c). This is the final commit.

The desired result would look like this:
<pre><code>
bladeRF-cli: Added command 'do_y' to support libbladeRF functionality Y (c)
libbladeRF: Added sample usage of Y to the API header                   (b)
libbladeRF: Added functionality Y                                       (a)
</code></pre>

### Patch ordering ###

Generally components of the bladeRF project should be updated in the following
order, from top to bottom. When your changes span multiple components, please
ensure they apply in the following order, or make it clear why they must be
applied in a different order.

 * FPGA code
 * FX3 code
 * libbladeRF
 * bladeRF-cli

This is intended to ensure that the system continues to operate with each
commit, which is important when bisecting the codebase to locate where defects
were introduced.

For example, if an FPGA change introduces a new feature for use by libbladeRF,
we would want that feature committed first, and then the associated libbladeRF
functionality added afterwards.


## Submitting Patches ##

Patches can be emailed to bladeRF@nuand.com. Please include `[PATCH]` in
the subject line.

Patches may also be submitted via GitHub pull requests. Please use a separate
pull request per patch set, and stage each patch set in a separate branch.

**Do not** continue committing changes to the branch you created a pull request from.
If you find that additional changes are required after submitting your pull request:

 * Close the pull request, noting the reason for closing it.
 * Make the necessary changes in your fork.
 * Rebase and squash your commits as requested in this document.
 * Open a new pull request with your updated changes.

Furthermore, **do not** commit unrelated changes to the branch used for your pull request.  Work on unrelated changes in a different branch.
