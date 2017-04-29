# Subtree Instructions

These sources are derived from a subtree of https://github.com/analogdevicesinc/no-OS.

## Updating the subtree

Fetch new vendor master:

```
git remote add adi-no-os https://github.com/analogdevicesinc/no-OS.git
git fetch adi-no-os master
git checkout adi-no-os/master
```

Update the `adi-subtree` subtree branch:

```
git subtree split --prefix=ad9361/sw/ -b adi-subtree
```

Merge the updated subtree:

```
git subtree merge -P host/libraries/libbladeRF/src/driver/thirdparty/adi adi-subtree --squash
```
