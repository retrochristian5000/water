# XENROLL.DLL: historical certificate-enrollment compatibility ledger

Status: **SOURCE-AUDITED / NOT IMPLEMENTED**. This is research evidence
for a potential Water component, not a claim of compiled or runtime support.

## Function and version boundary

`XENROLL.DLL` implements the pre-Vista **Certificate Enrollment Control**,
a COM/ActiveX component for generating certificate requests and accepting
issued certificate responses. It is separate from the lower-level CryptoAPI
functions and from the Vista-and-later `CertEnroll.dll` library.

| Context | Evidence | Confidence |
| --- | --- | --- |
| Windows 98 First Edition | Original Windows 98 CD inventory names `XENROLL.DLL`, 69,328 bytes, and the floppy CAB inventory locates it in `WIN98_35.CAB`. | Confirmed distributed |
| Internet Explorer 4-era package | Microsoft KB196704 inventory lists `Xenroll.dll` version `5.102.1680.101`, 69,328 bytes, dated May 8, 1998. | Confirmed package listing |
| Windows NT 4.0 SP4 | Archived SP4 **Alpha** file listing contains `XENROLL.DLL`, `XENROLL.AXP`, `XENROLL.CAB`, and `XENROLL.X86`; architecture-specific binary contents not inspected. | Confirmed distribution listing, no ABI comparison |
| Windows 2000 | Microsoft KB922706 describes availability in Windows 2000 and later pre-Vista versions. | Confirmed by Microsoft |
| Windows XP / Server 2003 | Microsoft describes certificate-enrollment Web pages invoking Xenroll for these clients. | Confirmed by Microsoft |
| Windows Vista / Server 2008 | Microsoft replaced/disabled Xenroll in favor of `CertEnroll.dll`. | Confirmed by Microsoft |
| Original Windows 95, 98 SE, Windows Me | Their **exact native distributions and binary versions** are not established by this ledger. Internet Explorer distribution may change what is installed. | Unverified |

**Do not infer:** equal filename means equal export table, CLSID, typelib,
registry registration, or security restrictions.

### Key historical transition: 2002 security update

Microsoft KB323172 documents a July 2002 version
`5.131.3659.0` (172,664 bytes). It explicitly distinguishes:

- old CEnroll CLSID: `{43F8F289-7A20-11D0-8F06-00C04FC295E1}`;
- updated Xenroll CLSID: `{127698E4-E730-4E5C-A2B1-21490A70C8A1}`.

These identities are **dated to the specific deployment contexts** in
KB323172, not a universal CLSID table for all Windows releases.
The file size change shows that the 1998 and 2002 editions cannot be
safely collapsed into one undocumented ABI.

The CERT vulnerability notice VU#3062 (December 2000) identifies the
older CEnroll control as permitting arbitrary file creation through a
scriptable request operation, even when marked safe for scripting.
**Never advertise unsafe methods as safe-for-scripting or silently
perform key/certificate installation without user authorization.**
Do not replicate the historical unsafe defaults.

## Interfaces: source-derived, not historical-version-certified

Microsoft's Xenroll-to-CertEnroll API mapping lists two evolving
interface groups:

- automation-oriented: `ICEnroll`, `ICEnroll2`, `ICEnroll3`,
  `ICEnroll4`;
- native-oriented: `IEnroll`, `IEnroll2`, `IEnroll4`.

Microsoft warns that these families diverged in functionality. It
does not follow that all of these interfaces existed in the **1998**
69,328-byte DLL. Use original type libraries / IDLs and OS-specific
tests before marking an interface present in a version ledger.

Capability areas in Microsoft's mapping include:

- PKCS #10 request construction; response acceptance (PKCS #7);
- cryptographic provider and key selection;
- certificate and key-store access;
- request attributes and extensions.

The names of COM methods/properties are **not equivalent to DLL PE
export names**. A COM class is obtained through the class factory,
not necessarily through an identically named exported function.

## Recovered Water implementation dependencies

At the time of audit (2026-10-09), Water's `master` tree contains
no `dlls/xenroll`, `dlls/certenroll`, or `include/xenroll.h`.

- `dlls/advapi32/advapi32.spec` already lists `CryptAcquireContextA/W`,
  `CryptGenKey`, `CryptSignHashA/W`, and `CryptExportKey`.
- `dlls/crypt32/crypt32.spec` lists `CryptEncodeObject[Ex]`,
  `CryptSignAndEncodeCertificate`, `CertOpenStore`,
  `CertCloseStore`, and `CertAddCertificateContextToStore`.
- `dlls/ole32` and `dlls/oleaut32` contain the existing COM and
  Automation infrastructure.

Export declarations demonstrate candidate building blocks only.
They do **not** establish that an entire enrollment workflow works.

## Verification gates before a clone

1. Acquire authorized, genuine comparison binaries for the 1998,
   2000, and 2002 versions; record SHA-256, machine/ISA, timestamp,
   file version, and source distribution for each.
2. Extract *real* PE export names and ordinals, typelib/IDL definitions,
   CLSIDs, IIDs, DISPIDs, threading model, and registration metadata.
3. Separate Win9x/Internet Explorer distribution from NT 4/2000/XP
   and from Vista's replacement. Do not synthesize a universal ABI.
4. Prototype the COM class factory and basic interface queries with
   clear `CLASS_E_CLASSNOTAVAILABLE` / `E_NOINTERFACE` behavior.
   Do **not** register a bogus fully functional class.
5. Progress from non-mutating provider enumeration to key generation
   and PKCS #10 encoding, then to confirmation-gated response import;
   preserve user consent and key-store isolation.
6. Add export, COM activation, typelib, and intentionally negative
   security tests for each verified target personality.
7. Only after source/build/runtime verification mark capabilities
   IMPLEMENTED. Preserve negative evidence and contradictory versions.

## Sources

- Windows 98 CD inventory (KB Q188434):
  https://helparchive.huntertur.net/document/106780
- Windows 98 floppy inventory (KB Q191057):
  https://helparchive.huntertur.net/document/106792
- Microsoft KB196704: IE4 system32 file inventory:
  https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/196/704.HTM
- NT 4 SP4 Alpha file inventory:
  https://ftp.zx.net.nz/pub/Patches/Microsoft/WinNT-patches/4.0/SP4CD/ALPHA/
- Microsoft KB922706:
  https://support.microsoft.com/en-us/topic/how-to-use-certificate-services-web-enrollment-pages-together-with-windows-vista-or-windows-server-2008-5c90d10b-ce1b-c966-e6fc-dafa1c919447
- Microsoft's interface-family mapping:
  https://learn.microsoft.com/en-us/windows/win32/seccertenroll/mapping-xenroll-dll-to-certenroll-dll
- Microsoft KB323172 archive (2002 security revision):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/323172
- CERT VU#3062 (unsafe scripting behavior):
  https://kb.cert.org/vuls/id/3062
