#ifndef __EB_SECURE_H__
#define __EB_SECURE_H__

#ifdef CONFIG_EB_SECURE
/* function name: aml_ebootkey_put
 * function: write ebootkey for chip
 * pefuse: encrypt data buf
 * dlen : buf size
 * return : fail: < 0; ok: >=0
 * */
extern int aml_ebootkey_put(void *pefuse, int dlen);
#else
static int aml_ebootkey_put(void *pefuse, int dlen)
{
	return -1;
}
#endif

#endif
