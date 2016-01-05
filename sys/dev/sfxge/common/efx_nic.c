/*-
 * Copyright (c) 2007-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"

	__checkReturn	efx_rc_t
efx_family(
	__in		uint16_t venid,
	__in		uint16_t devid,
	__out		efx_family_t *efp)
{
	if (venid == EFX_PCI_VENID_SFC) {
		switch (devid) {
#if EFSYS_OPT_FALCON
		case EFX_PCI_DEVID_FALCON:
			*efp = EFX_FAMILY_FALCON;
			return (0);
#endif
#if EFSYS_OPT_SIENA
		case EFX_PCI_DEVID_SIENA_F1_UNINIT:
			/*
			 * Hardware default for PF0 of uninitialised Siena.
			 * manftest must be able to cope with this device id.
			 */
			*efp = EFX_FAMILY_SIENA;
			return (0);

		case EFX_PCI_DEVID_BETHPAGE:
		case EFX_PCI_DEVID_SIENA:
			*efp = EFX_FAMILY_SIENA;
			return (0);
#endif

#if EFSYS_OPT_HUNTINGTON
		case EFX_PCI_DEVID_HUNTINGTON_PF_UNINIT:
			/*
			 * Hardware default for PF0 of uninitialised Huntington.
			 * manftest must be able to cope with this device id.
			 */
			*efp = EFX_FAMILY_HUNTINGTON;
			return (0);

		case EFX_PCI_DEVID_FARMINGDALE:
		case EFX_PCI_DEVID_GREENPORT:
			*efp = EFX_FAMILY_HUNTINGTON;
			return (0);

		case EFX_PCI_DEVID_FARMINGDALE_VF:
		case EFX_PCI_DEVID_GREENPORT_VF:
			*efp = EFX_FAMILY_HUNTINGTON;
			return (0);
#endif
		default:
			break;
		}
	}

	*efp = EFX_FAMILY_INVALID;
	return (ENOTSUP);
}

/*
 * To support clients which aren't provided with any PCI context infer
 * the hardware family by inspecting the hardware. Obviously the caller
 * must be damn sure they're really talking to a supported device.
 */
	__checkReturn	efx_rc_t
efx_infer_family(
	__in		efsys_bar_t *esbp,
	__out		efx_family_t *efp)
{
	efx_family_t family;
	efx_oword_t oword;
	unsigned int portnum;
	efx_rc_t rc;

	EFSYS_BAR_READO(esbp, FR_AZ_CS_DEBUG_REG_OFST, &oword, B_TRUE);
	portnum = EFX_OWORD_FIELD(oword, FRF_CZ_CS_PORT_NUM);
	switch (portnum) {
	case 0: {
		efx_dword_t dword;
		uint32_t hw_rev;

		EFSYS_BAR_READD(esbp, ER_DZ_BIU_HW_REV_ID_REG_OFST, &dword,
		    B_TRUE);
		hw_rev = EFX_DWORD_FIELD(dword, ERF_DZ_HW_REV_ID);
		if (hw_rev == ER_DZ_BIU_HW_REV_ID_REG_RESET) {
#if EFSYS_OPT_HUNTINGTON
			family = EFX_FAMILY_HUNTINGTON;
			break;
#endif
		} else {
#if EFSYS_OPT_FALCON
			family = EFX_FAMILY_FALCON;
			break;
#endif
		}
		rc = ENOTSUP;
		goto fail1;
	}

#if EFSYS_OPT_SIENA
	case 1:
	case 2:
		family = EFX_FAMILY_SIENA;
		break;
#endif
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	if (efp != NULL)
		*efp = family;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#define	EFX_BIU_MAGIC0	0x01234567
#define	EFX_BIU_MAGIC1	0xfedcba98

	__checkReturn	efx_rc_t
efx_nic_biu_test(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	efx_rc_t rc;

	/*
	 * Write magic values to scratch registers 0 and 1, then
	 * verify that the values were written correctly.  Interleave
	 * the accesses to ensure that the BIU is not just reading
	 * back the cached value that was last written.
	 */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC0);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC1);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC0) {
		rc = EIO;
		goto fail1;
	}

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC1) {
		rc = EIO;
		goto fail2;
	}

	/*
	 * Perform the same test, with the values swapped.  This
	 * ensures that subsequent tests don't start with the correct
	 * values already written into the scratch registers.
	 */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC1);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC0);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC1) {
		rc = EIO;
		goto fail3;
	}

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC0) {
		rc = EIO;
		goto fail4;
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_FALCON

static efx_nic_ops_t	__efx_nic_falcon_ops = {
	falcon_nic_probe,		/* eno_probe */
	NULL,				/* eno_set_drv_limits */
	falcon_nic_reset,		/* eno_reset */
	falcon_nic_init,		/* eno_init */
	NULL,				/* eno_get_vi_pool */
	NULL,				/* eno_get_bar_region */
#if EFSYS_OPT_DIAG
	falcon_sram_test,		/* eno_sram_test */
	falcon_nic_register_test,	/* eno_register_test */
#endif	/* EFSYS_OPT_DIAG */
	falcon_nic_fini,		/* eno_fini */
	falcon_nic_unprobe,		/* eno_unprobe */
};

#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA

static efx_nic_ops_t	__efx_nic_siena_ops = {
	siena_nic_probe,		/* eno_probe */
	NULL,				/* eno_set_drv_limits */
	siena_nic_reset,		/* eno_reset */
	siena_nic_init,			/* eno_init */
	NULL,				/* eno_get_vi_pool */
	NULL,				/* eno_get_bar_region */
#if EFSYS_OPT_DIAG
	siena_sram_test,		/* eno_sram_test */
	siena_nic_register_test,	/* eno_register_test */
#endif	/* EFSYS_OPT_DIAG */
	siena_nic_fini,			/* eno_fini */
	siena_nic_unprobe,		/* eno_unprobe */
};

#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON

static efx_nic_ops_t	__efx_nic_hunt_ops = {
	hunt_nic_probe,			/* eno_probe */
	hunt_nic_set_drv_limits,	/* eno_set_drv_limits */
	hunt_nic_reset,			/* eno_reset */
	hunt_nic_init,			/* eno_init */
	hunt_nic_get_vi_pool,		/* eno_get_vi_pool */
	hunt_nic_get_bar_region,	/* eno_get_bar_region */
#if EFSYS_OPT_DIAG
	hunt_sram_test,			/* eno_sram_test */
	hunt_nic_register_test,		/* eno_register_test */
#endif	/* EFSYS_OPT_DIAG */
	hunt_nic_fini,			/* eno_fini */
	hunt_nic_unprobe,		/* eno_unprobe */
};

#endif	/* EFSYS_OPT_HUNTINGTON */

	__checkReturn	efx_rc_t
efx_nic_create(
	__in		efx_family_t family,
	__in		efsys_identifier_t *esip,
	__in		efsys_bar_t *esbp,
	__in		efsys_lock_t *eslp,
	__deref_out	efx_nic_t **enpp)
{
	efx_nic_t *enp;
	efx_rc_t rc;

	EFSYS_ASSERT3U(family, >, EFX_FAMILY_INVALID);
	EFSYS_ASSERT3U(family, <, EFX_FAMILY_NTYPES);

	/* Allocate a NIC object */
	EFSYS_KMEM_ALLOC(esip, sizeof (efx_nic_t), enp);

	if (enp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	enp->en_magic = EFX_NIC_MAGIC;

	switch (family) {
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		enp->en_enop = (efx_nic_ops_t *)&__efx_nic_falcon_ops;
		enp->en_features = 0;
		break;
#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		enp->en_enop = (efx_nic_ops_t *)&__efx_nic_siena_ops;
		enp->en_features =
		    EFX_FEATURE_IPV6 |
		    EFX_FEATURE_LFSR_HASH_INSERT |
		    EFX_FEATURE_LINK_EVENTS |
		    EFX_FEATURE_PERIODIC_MAC_STATS |
		    EFX_FEATURE_WOL |
		    EFX_FEATURE_MCDI |
		    EFX_FEATURE_LOOKAHEAD_SPLIT |
		    EFX_FEATURE_MAC_HEADER_FILTERS |
		    EFX_FEATURE_TX_SRC_FILTERS;
		break;
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		enp->en_enop = (efx_nic_ops_t *)&__efx_nic_hunt_ops;
		/* FIXME: Add WOL support */
		enp->en_features =
		    EFX_FEATURE_IPV6 |
		    EFX_FEATURE_LINK_EVENTS |
		    EFX_FEATURE_PERIODIC_MAC_STATS |
		    EFX_FEATURE_MCDI |
		    EFX_FEATURE_MAC_HEADER_FILTERS |
		    EFX_FEATURE_MCDI_DMA |
		    EFX_FEATURE_PIO_BUFFERS |
		    EFX_FEATURE_FW_ASSISTED_TSO;
		break;
#endif	/* EFSYS_OPT_HUNTINGTON */

	default:
		rc = ENOTSUP;
		goto fail2;
	}

	enp->en_family = family;
	enp->en_esip = esip;
	enp->en_esbp = esbp;
	enp->en_eslp = eslp;

	*enpp = enp;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	enp->en_magic = 0;

	/* Free the NIC object */
	EFSYS_KMEM_FREE(esip, sizeof (efx_nic_t), enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
#if EFSYS_OPT_MCDI
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
#endif	/* EFSYS_OPT_MCDI */
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_PROBE));

	enop = enp->en_enop;
	if ((rc = enop->eno_probe(enp)) != 0)
		goto fail1;

	if ((rc = efx_phy_probe(enp)) != 0)
		goto fail2;

	enp->en_mod_flags |= EFX_MOD_PROBE;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	enop->eno_unprobe(enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_PCIE_TUNE

	__checkReturn	efx_rc_t
efx_nic_pcie_tune(
	__in		efx_nic_t *enp,
	unsigned int	nlanes)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

#if EFSYS_OPT_FALCON
	if (enp->en_family == EFX_FAMILY_FALCON)
		return (falcon_nic_pcie_tune(enp, nlanes));
#endif
	return (ENOTSUP);
}

	__checkReturn	efx_rc_t
efx_nic_pcie_extended_sync(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

#if EFSYS_OPT_SIENA
	if (enp->en_family == EFX_FAMILY_SIENA)
		return (siena_nic_pcie_extended_sync(enp));
#endif

	return (ENOTSUP);
}

#endif	/* EFSYS_OPT_PCIE_TUNE */

	__checkReturn	efx_rc_t
efx_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enop->eno_set_drv_limits != NULL) {
		if ((rc = enop->eno_set_drv_limits(enp, edlp)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (enop->eno_get_bar_region == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	if ((rc = (enop->eno_get_bar_region)(enp,
		    region, offsetp, sizep)) != 0) {
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *evq_countp,
	__out		uint32_t *rxq_countp,
	__out		uint32_t *txq_countp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (enop->eno_get_vi_pool != NULL) {
		uint32_t vi_count = 0;

		if ((rc = (enop->eno_get_vi_pool)(enp, &vi_count)) != 0)
			goto fail1;

		*evq_countp = vi_count;
		*rxq_countp = vi_count;
		*txq_countp = vi_count;
	} else {
		/* Use NIC limits as default value */
		*evq_countp = encp->enc_evq_limit;
		*rxq_countp = encp->enc_rxq_limit;
		*txq_countp = encp->enc_txq_limit;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_nic_init(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enp->en_mod_flags & EFX_MOD_NIC) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = enop->eno_init(enp)) != 0)
		goto fail2;

	enp->en_mod_flags |= EFX_MOD_NIC;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_nic_fini(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_NIC);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_INTR));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	enop->eno_fini(enp);

	enp->en_mod_flags &= ~EFX_MOD_NIC;
}

			void
efx_nic_unprobe(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
#if EFSYS_OPT_MCDI
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
#endif	/* EFSYS_OPT_MCDI */
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_INTR));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	efx_phy_unprobe(enp);

	enop->eno_unprobe(enp);

	enp->en_mod_flags &= ~EFX_MOD_PROBE;
}

			void
efx_nic_destroy(
	__in	efx_nic_t *enp)
{
	efsys_identifier_t *esip = enp->en_esip;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, ==, 0);

	enp->en_family = 0;
	enp->en_esip = NULL;
	enp->en_esbp = NULL;
	enp->en_eslp = NULL;

	enp->en_enop = NULL;

	enp->en_magic = 0;

	/* Free the NIC object */
	EFSYS_KMEM_FREE(esip, sizeof (efx_nic_t), enp);
}

	__checkReturn	efx_rc_t
efx_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	unsigned int mod_flags;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	/*
	 * All modules except the MCDI, PROBE, NVRAM, VPD, MON (which we
	 * do not reset here) must have been shut down or never initialized.
	 *
	 * A rule of thumb here is: If the controller or MC reboots, is *any*
	 * state lost. If it's lost and needs reapplying, then the module
	 * *must* not be initialised during the reset.
	 */
	mod_flags = enp->en_mod_flags;
	mod_flags &= ~(EFX_MOD_MCDI | EFX_MOD_PROBE | EFX_MOD_NVRAM |
		    EFX_MOD_VPD | EFX_MOD_MON);
	EFSYS_ASSERT3U(mod_flags, ==, 0);
	if (mod_flags != 0) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = enop->eno_reset(enp)) != 0)
		goto fail2;

	enp->en_reset_flags |= EFX_RESET_MAC;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			const efx_nic_cfg_t *
efx_nic_cfg_get(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	return (&(enp->en_nic_cfg));
}

#if EFSYS_OPT_DIAG

	__checkReturn	efx_rc_t
efx_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

	if ((rc = enop->eno_register_test(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_test_registers(
	__in		efx_nic_t *enp,
	__in		efx_register_set_t *rsp,
	__in		size_t count)
{
	unsigned int bit;
	efx_oword_t original;
	efx_oword_t reg;
	efx_oword_t buf;
	efx_rc_t rc;

	while (count > 0) {
		/* This function is only suitable for registers */
		EFSYS_ASSERT(rsp->rows == 1);

		/* bit sweep on and off */
		EFSYS_BAR_READO(enp->en_esbp, rsp->address, &original,
			    B_TRUE);
		for (bit = 0; bit < 128; bit++) {
			/* Is this bit in the mask? */
			if (~(rsp->mask.eo_u32[bit >> 5]) & (1 << bit))
				continue;

			/* Test this bit can be set in isolation */
			reg = original;
			EFX_AND_OWORD(reg, rsp->mask);
			EFX_SET_OWORD_BIT(reg, bit);

			EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &reg,
				    B_TRUE);
			EFSYS_BAR_READO(enp->en_esbp, rsp->address, &buf,
				    B_TRUE);

			EFX_AND_OWORD(buf, rsp->mask);
			if (memcmp(&reg, &buf, sizeof (reg))) {
				rc = EIO;
				goto fail1;
			}

			/* Test this bit can be cleared in isolation */
			EFX_OR_OWORD(reg, rsp->mask);
			EFX_CLEAR_OWORD_BIT(reg, bit);

			EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &reg,
				    B_TRUE);
			EFSYS_BAR_READO(enp->en_esbp, rsp->address, &buf,
				    B_TRUE);

			EFX_AND_OWORD(buf, rsp->mask);
			if (memcmp(&reg, &buf, sizeof (reg))) {
				rc = EIO;
				goto fail2;
			}
		}

		/* Restore the old value */
		EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &original,
			    B_TRUE);

		--count;
		++rsp;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	/* Restore the old value */
	EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &original, B_TRUE);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_test_tables(
	__in		efx_nic_t *enp,
	__in		efx_register_set_t *rsp,
	__in		efx_pattern_type_t pattern,
	__in		size_t count)
{
	efx_sram_pattern_fn_t func;
	unsigned int index;
	unsigned int address;
	efx_oword_t reg;
	efx_oword_t buf;
	efx_rc_t rc;

	EFSYS_ASSERT(pattern < EFX_PATTERN_NTYPES);
	func = __efx_sram_pattern_fns[pattern];

	while (count > 0) {
		/* Write */
		address = rsp->address;
		for (index = 0; index < rsp->rows; ++index) {
			func(2 * index + 0, B_FALSE, &reg.eo_qword[0]);
			func(2 * index + 1, B_FALSE, &reg.eo_qword[1]);
			EFX_AND_OWORD(reg, rsp->mask);
			EFSYS_BAR_WRITEO(enp->en_esbp, address, &reg, B_TRUE);

			address += rsp->step;
		}

		/* Read */
		address = rsp->address;
		for (index = 0; index < rsp->rows; ++index) {
			func(2 * index + 0, B_FALSE, &reg.eo_qword[0]);
			func(2 * index + 1, B_FALSE, &reg.eo_qword[1]);
			EFX_AND_OWORD(reg, rsp->mask);
			EFSYS_BAR_READO(enp->en_esbp, address, &buf, B_TRUE);
			if (memcmp(&reg, &buf, sizeof (reg))) {
				rc = EIO;
				goto fail1;
			}

			address += rsp->step;
		}

		++rsp;
		--count;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

#if EFSYS_OPT_LOOPBACK

extern			void
efx_loopback_mask(
	__in	efx_loopback_kind_t loopback_kind,
	__out	efx_qword_t *maskp)
{
	efx_qword_t mask;

	EFSYS_ASSERT3U(loopback_kind, <, EFX_LOOPBACK_NKINDS);
	EFSYS_ASSERT(maskp != NULL);

	/* Assert the MC_CMD_LOOPBACK and EFX_LOOPBACK namespace agree */
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_NONE == EFX_LOOPBACK_OFF);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_DATA == EFX_LOOPBACK_DATA);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_GMAC == EFX_LOOPBACK_GMAC);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XGMII == EFX_LOOPBACK_XGMII);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XGXS == EFX_LOOPBACK_XGXS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XAUI == EFX_LOOPBACK_XAUI);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_GMII == EFX_LOOPBACK_GMII);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SGMII == EFX_LOOPBACK_SGMII);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XGBR == EFX_LOOPBACK_XGBR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XFI == EFX_LOOPBACK_XFI);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XAUI_FAR == EFX_LOOPBACK_XAUI_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_GMII_FAR == EFX_LOOPBACK_GMII_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SGMII_FAR == EFX_LOOPBACK_SGMII_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XFI_FAR == EFX_LOOPBACK_XFI_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_GPHY == EFX_LOOPBACK_GPHY);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_PHYXS == EFX_LOOPBACK_PHY_XS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_PCS == EFX_LOOPBACK_PCS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_PMAPMD == EFX_LOOPBACK_PMA_PMD);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XPORT == EFX_LOOPBACK_XPORT);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XGMII_WS == EFX_LOOPBACK_XGMII_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XAUI_WS == EFX_LOOPBACK_XAUI_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XAUI_WS_FAR ==
	    EFX_LOOPBACK_XAUI_WS_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XAUI_WS_NEAR ==
	    EFX_LOOPBACK_XAUI_WS_NEAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_GMII_WS == EFX_LOOPBACK_GMII_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XFI_WS == EFX_LOOPBACK_XFI_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_XFI_WS_FAR ==
	    EFX_LOOPBACK_XFI_WS_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_PHYXS_WS == EFX_LOOPBACK_PHYXS_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_PMA_INT == EFX_LOOPBACK_PMA_INT);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SD_NEAR == EFX_LOOPBACK_SD_NEAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SD_FAR == EFX_LOOPBACK_SD_FAR);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_PMA_INT_WS ==
	    EFX_LOOPBACK_PMA_INT_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SD_FEP2_WS ==
	    EFX_LOOPBACK_SD_FEP2_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SD_FEP1_5_WS ==
	    EFX_LOOPBACK_SD_FEP1_5_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SD_FEP_WS == EFX_LOOPBACK_SD_FEP_WS);
	EFX_STATIC_ASSERT(MC_CMD_LOOPBACK_SD_FES_WS == EFX_LOOPBACK_SD_FES_WS);

	/* Build bitmask of possible loopback types */
	EFX_ZERO_QWORD(mask);

	if ((loopback_kind == EFX_LOOPBACK_KIND_OFF) ||
	    (loopback_kind == EFX_LOOPBACK_KIND_ALL)) {
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_OFF);
	}

	if ((loopback_kind == EFX_LOOPBACK_KIND_MAC) ||
	    (loopback_kind == EFX_LOOPBACK_KIND_ALL)) {
		/*
		 * The "MAC" grouping has historically been used by drivers to
		 * mean loopbacks supported by on-chip hardware. Keep that
		 * meaning here, and include on-chip PHY layer loopbacks.
		 */
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_DATA);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_GMAC);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XGMII);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XGXS);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XAUI);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_GMII);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_SGMII);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XGBR);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XFI);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XAUI_FAR);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_GMII_FAR);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_SGMII_FAR);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_XFI_FAR);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_PMA_INT);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_SD_NEAR);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_SD_FAR);
	}

	if ((loopback_kind == EFX_LOOPBACK_KIND_PHY) ||
	    (loopback_kind == EFX_LOOPBACK_KIND_ALL)) {
		/*
		 * The "PHY" grouping has historically been used by drivers to
		 * mean loopbacks supported by off-chip hardware. Keep that
		 * meaning here.
		 */
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_GPHY);
		EFX_SET_QWORD_BIT(mask,	EFX_LOOPBACK_PHY_XS);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_PCS);
		EFX_SET_QWORD_BIT(mask, EFX_LOOPBACK_PMA_PMD);
	}

	*maskp = mask;
}

	__checkReturn	efx_rc_t
efx_mcdi_get_loopback_modes(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_LOOPBACK_MODES_IN_LEN,
			    MC_CMD_GET_LOOPBACK_MODES_OUT_LEN)];
	efx_qword_t mask;
	efx_qword_t modes;
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_LOOPBACK_MODES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_LOOPBACK_MODES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_LOOPBACK_MODES_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used <
	    MC_CMD_GET_LOOPBACK_MODES_OUT_SUGGESTED_OFST +
	    MC_CMD_GET_LOOPBACK_MODES_OUT_SUGGESTED_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	/*
	 * We assert the MC_CMD_LOOPBACK and EFX_LOOPBACK namespaces agree
	 * in efx_loopback_mask() and in siena_phy.c:siena_phy_get_link().
	 */
	efx_loopback_mask(EFX_LOOPBACK_KIND_ALL, &mask);

	EFX_AND_QWORD(mask,
	    *MCDI_OUT2(req, efx_qword_t, GET_LOOPBACK_MODES_OUT_SUGGESTED));

	modes = *MCDI_OUT2(req, efx_qword_t, GET_LOOPBACK_MODES_OUT_100M);
	EFX_AND_QWORD(modes, mask);
	encp->enc_loopback_types[EFX_LINK_100FDX] = modes;

	modes = *MCDI_OUT2(req, efx_qword_t, GET_LOOPBACK_MODES_OUT_1G);
	EFX_AND_QWORD(modes, mask);
	encp->enc_loopback_types[EFX_LINK_1000FDX] = modes;

	modes = *MCDI_OUT2(req, efx_qword_t, GET_LOOPBACK_MODES_OUT_10G);
	EFX_AND_QWORD(modes, mask);
	encp->enc_loopback_types[EFX_LINK_10000FDX] = modes;

	if (req.emr_out_length_used >=
	    MC_CMD_GET_LOOPBACK_MODES_OUT_40G_OFST +
	    MC_CMD_GET_LOOPBACK_MODES_OUT_40G_LEN) {
		/* Response includes 40G loopback modes */
		modes =
		    *MCDI_OUT2(req, efx_qword_t, GET_LOOPBACK_MODES_OUT_40G);
		EFX_AND_QWORD(modes, mask);
		encp->enc_loopback_types[EFX_LINK_40000FDX] = modes;
	}

	EFX_ZERO_QWORD(modes);
	EFX_SET_QWORD_BIT(modes, EFX_LOOPBACK_OFF);
	EFX_OR_QWORD(modes, encp->enc_loopback_types[EFX_LINK_100FDX]);
	EFX_OR_QWORD(modes, encp->enc_loopback_types[EFX_LINK_1000FDX]);
	EFX_OR_QWORD(modes, encp->enc_loopback_types[EFX_LINK_10000FDX]);
	EFX_OR_QWORD(modes, encp->enc_loopback_types[EFX_LINK_40000FDX]);
	encp->enc_loopback_types[EFX_LINK_UNKNOWN] = modes;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif /* EFSYS_OPT_LOOPBACK */
