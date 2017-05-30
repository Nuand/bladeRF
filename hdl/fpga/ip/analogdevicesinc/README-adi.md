# Subtree Instructions

The hdl/ subdirectory is a git subtree.

## Creating the subtree

Clear `hdl/fpga/ip/analogdevicesinc/hdl` to pave the way for the subtree:

```
rm -rf hdl/fpga/ip/analogdevicesinc/hdl
git add hdl/
git commit -m 'hdl/analogdevicesinc: remove manual adi hdl codebase integration'
```

Fetch vendor master:

```
git remote add adi-hdl https://github.com/analogdevicesinc/hdl.git
git fetch adi-hdl master
```

Add the subtree:

```
git subtree add -P hdl/fpga/ip/analogdevicesinc/hdl adi-hdl/master --squash
```

Prune unneeded subdirectories:

```
cd hdl/fpga/ip/analogdevicesinc/hdl
git rm -r library/* projects/*
git reset HEAD library/axi_ad9361 library/axi_dmac library/common library/scripts \
    library/util_axis_fifo library/util_axis_resize library/util_cpack library/util_upack \
    projects/arradio
git checkout -- library/axi_ad9361 library/axi_dmac library/common library/scripts \
    library/util_axis_fifo library/util_axis_resize library/util_cpack library/util_upack \
    projects/arradio
git commit -m 'hdl/analogdevicesinc: prune unnecessary subdirectories from ADI repo'
```

Replay our modifications to the vendor code, omitting the oldest commit (1ed07b8, a commit
of a copy of the ADI repo) and the three most recent commits (the rm of the hdl tree, and the
two subtree merges):

```
git cherry-pick -X ours \
    $(git rev-list --reverse HEAD~3 ^1ed07b8 hdl/fpga/ip/analogdevicesinc/hdl)
```

## Updating the subtree

Fetch new vendor master:

```
git remote add adi-hdl https://github.com/analogdevicesinc/hdl.git
git fetch adi-hdl master
```

Merge the updated subtree:

```
git subtree merge -P hdl/fpga/ip/analogdevicesinc/hdl adi-hdl/master --squash
```
