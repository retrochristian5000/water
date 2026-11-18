/* WHP Windows 95 RTM HDA controller *research* register boundary.
 *
 * Controller register offsets derive from the Intel HD Audio specification.
 * This is a portable numeric reference, NOT an original Win95 RTM DDK
 * header, codec driver, or promise of HDA support under Windows 95.
 * A Win16 NE audio .DRV, Win9x i386 LE VxD and NT/WDM .SYS have
 * independent ABIs. No MMIO mapping, access or write helpers live here.
 *
 * Reference: Intel High Definition Audio Specification rev. 1.0a,
 * sections 3.2-3.3.
 */
#ifndef WHP_WIN95RTM_AUDIO_HDA_REGS_H
#define WHP_WIN95RTM_AUDIO_HDA_REGS_H

/* PCI type-0 controller class tuple, not an HDA codec identity. */
#define WHP_WIN95_HDA_PCI_BASE_CLASS         0x04u
#define WHP_WIN95_HDA_PCI_SUBCLASS           0x03u
#define WHP_WIN95_HDA_PCI_PROGIF             0x00u

/* Offsets in the *mapped HDA controller register block*, not PCI config. */
#define WHP_WIN95_HDA_REG_GCAP               0x00u
#define WHP_WIN95_HDA_REG_VMIN               0x02u
#define WHP_WIN95_HDA_REG_VMAJ               0x03u
#define WHP_WIN95_HDA_REG_GCTL               0x08u
#define WHP_WIN95_HDA_REG_STATESTS           0x0eu
#define WHP_WIN95_HDA_REG_INTCTL             0x20u
#define WHP_WIN95_HDA_REG_INTSTS             0x24u
#define WHP_WIN95_HDA_REG_CORBLBASE          0x40u
#define WHP_WIN95_HDA_REG_CORBUBASE          0x44u
#define WHP_WIN95_HDA_REG_CORBWP             0x48u
#define WHP_WIN95_HDA_REG_CORBRP             0x4au
#define WHP_WIN95_HDA_REG_CORBCTL            0x4cu
#define WHP_WIN95_HDA_REG_CORBSTS            0x4du
#define WHP_WIN95_HDA_REG_CORBSIZE           0x4eu
#define WHP_WIN95_HDA_REG_RIRBLBASE          0x50u
#define WHP_WIN95_HDA_REG_RIRBUBASE          0x54u
#define WHP_WIN95_HDA_REG_RIRBWP             0x58u
#define WHP_WIN95_HDA_REG_RINTCNT            0x5au
#define WHP_WIN95_HDA_REG_RIRBCTL            0x5cu
#define WHP_WIN95_HDA_REG_RIRBSTS            0x5du
#define WHP_WIN95_HDA_REG_RIRBSIZE           0x5eu

/* GCAP: output/input/bidirectional stream counts; serial data outputs. */
#define WHP_WIN95_HDA_GCAP_64OK              0x0001u
#define WHP_WIN95_HDA_GCAP_NSDO_MASK         0x0006u
#define WHP_WIN95_HDA_GCAP_NSDO_1            0x0000u
#define WHP_WIN95_HDA_GCAP_NSDO_2            0x0002u
#define WHP_WIN95_HDA_GCAP_NSDO_4            0x0004u
#define WHP_WIN95_HDA_GCAP_NSDO_RESERVED     0x0006u
#define WHP_WIN95_HDA_GCAP_BSS_MASK          0x00f8u
#define WHP_WIN95_HDA_GCAP_ISS_MASK          0x0f00u
#define WHP_WIN95_HDA_GCAP_OSS_MASK          0xf000u

/* CORBSIZE/RIRBSIZE: bits 4-6 report available ring sizes.
 * Bits 0-1 select ring size: never confuse selected with supported.
 */
#define WHP_WIN95_HDA_RING_SIZE_SEL_MASK     0x03u
#define WHP_WIN95_HDA_RING_CAP_2             0x10u
#define WHP_WIN95_HDA_RING_CAP_16            0x20u
#define WHP_WIN95_HDA_RING_CAP_256           0x40u

#endif /* WHP_WIN95RTM_AUDIO_HDA_REGS_H */
