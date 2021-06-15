/*
 *
 * sample.c
 * Monday, 4/13/1998.
 *
 *
 * Sample file.
 * Reads 2 file types containing fixed column data
 * Column positions and sizes stored in static arrays
 * Data records read in and converted to PPSZ structures
 * Data written in CSV format.
 *
 * a PSZ means a (char *) which is a normal NULL terminated string
 * a PPSZ is a NULL terminated array of string pointers
 *
 * ppsz ------>|---|    --------------
 *             |psz|--->|characters\0|
 *             |---|    --------------
 *             |psz|
 *             |---|
 *             |psz|
 *             |---|
 *             |psz|
 *             |---|
 *             |NUL|
 *             |---|
 *
 * The function MakePPSZ makes a ppsz from a row of data and 
 * a column-description
 *
 * The function MemFreePPSZ frees the PPSZ when you are done
 *
 */

#include <dos.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// Typedefs and defines moved from gnutype.h
//

#define CHAR  char
typedef unsigned char UCHAR;	
typedef UCHAR *PSZ;
typedef PSZ   *PPSZ;
typedef unsigned int UINT;
typedef UINT  *PUINT;
typedef UINT  BOOL;
#define TRUE  1
#define FALSE 0


#define OUTPUTFILENAME "OUTFILE.CSV"


/*
 * Structure that holds the definition of the columns 
 * of the input files
 */
typedef struct
   {
   UINT uCol;
   UINT uLen;
   PSZ  pszColName;
   } COL;
typedef COL *PCOL;

//
// E file description 
//
//                                         Column
//Col  Len    Wincaps field                Index  CAS field description     CAS field            Remarks
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
COL pECOLS [] =
{{1  ,  4 ,  "Contract Number"          }, // 0   Contract ID               CONTRACT.CONTID      
 {5  ,  4 ,  "Project Number"           }, // 1   Project ID                CONTRACT.CNTLPCN     
 {9  ,  1 ,  "Estimate Key"             }, // 2   Voucher Status            VOUCHER.VOSTAT       Populated only when voucher is final.
 {10 ,  2 ,  "Estimate Number"          }, // 3   Voucher Number            VOUCHER.VOUCHER      
 {12 ,  6 ,  "Estimate Date"            }, // 4   Voucher Date              VOUCHER.VODTDATE     Needs to be formatted YYMMDD.
 {18 ,  4 ,  "Record Type"              }, // 5   Item Type                 PROJITEM.PITYPE      Can be ITEM, SPPV, CO, ROY, CONU, MAT, ADJ, FA, NP, SPT, LD, X.
 {22 ,  4 ,  "Item Key"                 }, // 6   Part of Item Number           
 {26 ,  8 ,  "Item Code"                }, // 7   Item Number               ITEMLIST.ITEM        Created from ITEMLIST.ALTITMID and ITEMLIST.ILSST.
 {34 ,  4 ,  "Unit of Measure"          }, // 8   Item Unit                 ITEMLIST.IUNITS      Ignore unless item is new. Then use it to create ITEMLIST record.
 {38 ,  25,  "Item Description1"        }, // 9   Item Description          ITEMLIST.IDESCR      Ignore unless item is new. Then use it to create ITEMLIST record.
 {63 ,  25,  "Item Description2"        }, // 10  Supplemental Description  PROJITEM.PISUPDSC    Use to create Supplemental Description
 {88 ,  14,  "Bid Quantity"             }, // 11                                                 Refer to Design Document
 {102,  14,  "Authorized Quantity"      }, // 12                                                 Refer to Design Document
 {116,  10,  "Bidder Unit Price"        }, // 13  Proposal Item Price       PROJITEM.PIPRICE     Use in PROJITEM record.
 {160,  14,  "On-hand Invoice Quant."   }, // 14                                                 Material on-hand quantity.
 {174,  14,  "On-hand Invoice Price"    }, // 15  Material Allowance Price  MATALW.MATPRC        Material on-hand price.
 {188,  15,  "Estimate Quantity to Date"}, // 16  Quantity in Place         PROJITEM.PIQTYPLC    Use in PROJITEM record.
 {259,  2 ,  "Item percent retained"    }, // 17  Contract retainage                             Compare to the latest CONTRET record.  If  different then build a new CONTRET record.
 {339,  3 ,  "Material Source"          }, // 18  Generic field in CONTMOD                       CONTMOD.CMSST51                    Add to CONTMOD record
 {342,  3 ,  "Material Source"          }, // 19  Generic field in CONTMOD                       CONTMOD.CMSST52                    Add to CONTMOD record
 {345,  1 ,  "Material Source"          }, // 20  Generic field in CONTMOD                       CONTMOD.CMFLG51                    Add to CONTMOD record
 {386,  10,  ""                         }, // 21
 {0  ,  0 ,  NULL                       }};//  Terminating Record


//
// L file description 
//
//Col  Len    Wincaps field                Index  CAS field description     CAS field            Remarks
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
COL pLCOLS [] =
{{1  ,  10,  "Unknown"                  }, // 0   No Idea
 {11  , 10,  "Unknown"                  }, // 1   No Idea
 {0  ,  0 ,  NULL                       }};//     Terminating Record



CHAR sz  [1024]; // general purpose buffer
CHAR sz2 [1024]; // general purpose buffer



/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

/*
 * Prints an error and dies
 * from GnuMisc
 */
UINT _cdecl Error (PSZ psz, ...)
   {
   va_list vlst;

   printf ("Error: ");
   va_start (vlst, psz);
   vprintf (psz, vlst);
   va_end (vlst);
   printf ("\n");
   exit (1);
   return 0;
   }

/*
 * Frees a PPSZ
 * from GnuMem
 */
void MemFreePPSZ (PSZ *ppsz, UINT uNum)
   {
   if (!ppsz)
      return;
   if (uNum)
      {
      while (uNum)
         if (ppsz [--uNum])
            free (ppsz [uNum]);
      }
   free (ppsz);
   }


/*
 * Converts a string to a CSV string
 * from GnuStr
 */
PSZ StrMakeCSVField (PSZ pszDest, PSZ pszSrc)
   {
   PSZ  psz;

   psz  = pszDest;
   *psz = '\0';

   if (!pszSrc)
      return pszDest;

   if (strchr (pszSrc, '"'))
      {
      *psz++ = '"';
      while (*pszSrc)
         {
         *psz++ = *pszSrc;
         if (*pszSrc++ == '"')
            *psz++ = '"';
         }
      *psz++ = '"';
      *psz   = '\0';
      }
   else if (strchr (pszSrc, ','))
      {
      *psz++ = '"';
      strcpy (psz, pszSrc);
      psz += strlen(pszSrc);
      *psz++ = '"';
      *psz = '\0';
      }
   else
      strcpy (psz, pszSrc);
   return pszDest;
   }


/*
 * This FN determines if a file exists in the current directory
 * given a wild card spec.  THIS FN IS OS DEPENDENT!
 * You will need to modify this fn to run on non win/dos systems
 */
BOOL FileExists (PSZ pszMatch, PSZ pszName)
   {
   struct find_t fInfo;

   if (_dos_findfirst (pszMatch, _A_NORMAL, &fInfo))
      return FALSE;

   strcpy (pszName, fInfo.name);
   return TRUE;
   }


/*
 * makes a PPSZ from a row of data
 * pLCOLS describes the file format
 *
 */
PPSZ MakePPSZ (PCOL pLCOLS, PSZ psz, PUINT puCols)
   {
   PPSZ ppsz;
   UINT i, uCols;

   for (uCols=0; pLCOLS[uCols].pszColName; uCols++)
      ;

   ppsz = calloc (uCols + 1, sizeof (PSZ));

   for (i=0; i<uCols; i++)
      {
      strncpy (sz2, psz + pLCOLS[i].uCol - 1, pLCOLS[i].uLen);
      sz2 [pLCOLS[i].uLen] = '\0';
      ppsz[i] = strdup (sz2);
      }
   ppsz[uCols] = NULL;

   *puCols = uCols;
   return ppsz;
   }


/*
 * Read a line from the L file
 *
 */
PPSZ ReadLLine (FILE *fp, PUINT puCols)
   {
   if (!fgets (sz, sizeof (sz), fp))
      return NULL;

   return MakePPSZ (pLCOLS, sz, puCols);
   }


/*
 * Read a line from the E file
 *
 */
PPSZ ReadELine (FILE *fp, PUINT puCols)
   {
   if (!fgets (sz, sizeof (sz), fp))
      return NULL;

   return MakePPSZ (pECOLS, sz, puCols);
   }


/*
 * reads L and E file, writes output records
 * Call once for each L/E pair of files
 */
void Translate (FILE *fpOut, PSZ pszLFile, PSZ pszEFile)
   {
   UINT uLCols, uECols;
   PPSZ ppszL, ppszE;
   FILE *fpL, *fpE;
   CHAR szItemNum [128];

   if (!(fpL = fopen (pszLFile, "r")))
      Error ("Could not open Ledger File %s\n", pszEFile);

   if (!(fpE = fopen (pszEFile, "r")))
      Error ("Could not open Estimate File %s\n", pszEFile);

   while (TRUE)
      {
      ppszL = ReadLLine (fpL, &uLCols);
      ppszE = ReadELine (fpE, &uECols);

      if (!ppszL || !ppszE)
         break;

      /*--- do work ---*/

      // Item number is ItemCode and ItemKey
      sprintf (szItemNum, "%s%s", ppszE[7], ppszE[6]);
      // ...


      /*--- write output ---*/
      fprintf (fpOut, " %s,",  StrMakeCSVField (sz2, szItemNum));
      fprintf (fpOut, " %s,",  StrMakeCSVField (sz2, ppszE[0]));
      fprintf (fpOut, " %s,",  StrMakeCSVField (sz2, ppszE[1]));
      fprintf (fpOut, " %s,",  StrMakeCSVField (sz2, ppszE[2]));
      fprintf (fpOut, " %s,",  StrMakeCSVField (sz2, ppszE[3]));
      fprintf (fpOut, " %s\n", StrMakeCSVField (sz2, ppszE[4]));

      /*--- free row data ---*/
      MemFreePPSZ (ppszL, uLCols);
      MemFreePPSZ (ppszE, uECols);
      }
   fclose (fpL);
   fclose (fpE);
   }


/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/


int _cdecl main (int argc, char *argv[])
   {
   CHAR szMatchName[256];
   CHAR szLName[256];
   CHAR szEName[256];
   FILE *fpOut;
   UINT i, uFiles = 0;

   if (!(fpOut = fopen (OUTPUTFILENAME, "w")))
      Error ("Could not open output file %s\n", OUTPUTFILENAME);

   for (i=1; i<100; i++)
      {
      sprintf (szMatchName, "C????L%d", i);
      if (!FileExists (szMatchName, szLName))
         continue;

      strcpy (szEName, szLName);
      szEName[5] = 'E';

      Translate (fpOut, szLName, szEName);

      uFiles += 2;
      }
   fclose (fpOut);
   printf ("%d files processed\n", uFiles);
   return 0;
   }

