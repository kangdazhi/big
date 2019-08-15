Baremetal VM
------------
Transform the vanilla guest to the baremetal VM.

Install the DID Device in the Guest
-----------------------------------
- Assume we are in the guest branch of Big repository (big/),
  which has the following files.
  - bonding-driver/
  - did/
  - Makefile
  - README.txt
- Make the modules.
  # make
- Insert the DID device.
  # make insert
  # ls /dev/did
