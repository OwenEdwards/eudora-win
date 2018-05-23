/*
 *  Functions to manage emsMIMEtype structures
 *  for use with Eudora EMS API under MS Windows.
 *
 *  Filename: mimetype.cpp
 *
 *  Last Edited: Wednesday, October 2, 1996
 *
 *  Author: Scott Manjourides
 *
 *  Copyright 1995, 1996 QUALCOMM Inc.
 Copyright (c) 2016, Computer History Museum 
All rights reserved. 
Redistribution and use in source and binary forms, with or without modification, are permitted (subject to 
the limitations in the disclaimer below) provided that the following conditions are met: 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
   disclaimer in the documentation and/or other materials provided with the distribution. 
 * Neither the name of Computer History Museum nor the names of its contributors may be used to endorse or promote products 
   derived from this software without specific prior written permission. 
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE 
COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
DAMAGE. 


 *
 *  Send comments and questions to <emsapi-info@qualcomm.com>
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>

#include "ems-win.h"
#include "mimetype.h"
#include "rfc822.h"

/* ========================================================================= */

#define safefree(p) { if (p) { free(p); p = NULL; } }

/* ========================================================================= */

static void free_param_type(emsMIMEParamP paramPtr);

/* ========================================================================= */

/*
 *  Create an emsMIMEtype structure to hold MIME information. Only the
 *  type and subtype are set here, use add_mime_parameter() to add
 *  any name/value parameter pairs.
 *
 *  NOTE: All input strings are COPIED before permanent use. The user
 *        of this function is responsible for calling free_mime_type()
 *        on returned structure.
 *
 *  Args:
 *   mime_type    [IN] the main MIME type: e.g., text, application, image
 *   sub_type     [IN] the sub type: e.g., plain, octet-stream, jpeg
 *   mime_version [IN] the MIME verion number, if NULL will default to "1.0"
 *
 *  Returns: Pointer to the created emsMIMEtype structure, NULL if error.
 */
emsMIMEtypeP make_mime_type(const char *mime_type, const char *sub_type, const char *mime_version)
{
	emsMIMEtypeP mimePtr = NULL;

	mimePtr = (emsMIMEtypeP) malloc(sizeof(emsMIMEtype));

	if ((mimePtr) && (mime_type) && (sub_type))
	{
		mimePtr->type = strdup(mime_type);
		mimePtr->subType = strdup(sub_type);

		if (mime_version)
			mimePtr->version = strdup(mime_version);
		else
			mimePtr->version = strdup("1.0");

		mimePtr->params = NULL;

		if ((mimePtr->type) && (mimePtr->subType) && (mimePtr->version))
		{
			return mimePtr;
		}
	}

	// If we get here then something went wrong
	// Do complete cleanup and return NULL

	free_mime_type(mimePtr);

	return (NULL); // ERROR
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Create an emsMIMEtype structure to hold MIME information. Structure is
 *  initialized to values provided in RFC822 content-type header line. This
 *  includes all parameter name-value pairs.
 *   
 *  NOTE: The user of this function is responsible for calling
 *        free_mime_type() on returned structure.
 *
 *  Args:
 *   content_type [IN] The RFC822 content-type string to parse
 *
 *  Returns: Pointer to the created emsMIMEtype structure, NULL if error.
 */
emsMIMEtypeP parse_make_mime_type(const char *content_type)
{
	const char *kPrefixStr = "Content-Type:";
	const unsigned int kPrefixStrLen = strlen(kPrefixStr);

	if (strnicmp(content_type, kPrefixStr, kPrefixStrLen) != 0)
		return NULL;

	char *cp = (char *) content_type + kPrefixStrLen;
	char *mime_type = NULL, *mime_subtype = NULL;

	mime_type = rfc822_extract_token(&cp);

	if ((strlen(mime_type) > 0) && ((*cp++) == '/'))
	{
		mime_subtype = rfc822_extract_token(&cp);

		if (strlen(mime_subtype) > 0)
		{
			// We have a type/subtype, so create emsMIMEtype structure
			emsMIMEtypeP mimePtr = make_mime_type(mime_type, mime_subtype, NULL);

			safefree(mime_type);
			safefree(mime_subtype);

			if (mimePtr)
			{
				char *name = NULL, *value = NULL;

				do {

					if ((*cp++) != ';') // Skip semi-colon
						break;
				
					if (!(name = rfc822_extract_token(&cp)))
						break;

					if ((*cp++) != '=') // Skip equals
						break;

					if (!(value = rfc822_extract_token(&cp)))
						break;

					if ((strlen(name) > 0) && (strlen(value) > 0))
						add_mime_parameter(mimePtr, name, value);

					safefree(name);
					safefree(value);
				} while (*cp);

				safefree(name);
				safefree(value);

				return mimePtr;
			}
		}
	}

	// If we get here then something went wrong
	// Do complete cleanup

	safefree(mime_type);
	safefree(mime_subtype);

	return (NULL); // ERROR
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Add a parameter to an existing emsMIMEtype structure. This structure
 *  should be created using make_mime_type().
 *
 *  NOTE: All input strings are COPIED before permanent use.
 *
 * Args:
 *   mimePtr [IN] Pointer to the emsMIMEtype structure to be added too
 *   name    [IN] Name of the parameter
 *   value   [IN] Value of the parameter
 *   
 *  Returns: Boolean (TRUE = success, FALSE = failure)
 */
int add_mime_parameter(emsMIMEtypeP mimePtr, const char *name, const char *value)
{
	if (!mimePtr)
		return (FALSE);
	
	emsMIMEParamP paramPtr = (emsMIMEParamP) malloc (sizeof(emsMIMEparam));

	if (paramPtr)
	{
		paramPtr->name = strdup(name);
		paramPtr->value = strdup(value);
		paramPtr->next = NULL;

		if ((paramPtr->name) && (paramPtr->value))
		{
			emsMIMEParamP *paramEnd = &(mimePtr->params);

		    while (*paramEnd)
				paramEnd = &((*paramEnd)->next);

			*paramEnd = paramPtr;

			return (TRUE);
		}
	}

	// If we get here then something went wrong
	// Do complete cleanup

	free_param_type(paramPtr);
	return (FALSE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Free an emsMIMEtype structure, including all strings and parameters.
 *
 *  Args:
 *   mimePtr [IN] Pointer to the emsMIMEtype structure to be freed
 *
 *  No return value.
 */
void free_mime_type(emsMIMEtypeP mimePtr)
{
	if (mimePtr)
	{
		safefree(mimePtr->type);
		safefree(mimePtr->subType);
		safefree(mimePtr->version);
		
		free_param_type(mimePtr->params);

		safefree(mimePtr);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Convert an emsMIMEtype structure to a Content-Type header field line
 *  in the format specified by RFC 882.
 *
 *  NOTE: The user of this function is responsible for freeing the
 *        returned string.
 *
 *  Args:
 *   mimePtr [IN] Pointer to the emsMIMEtype structure
 *
 *  Returns: String containing header field; dynamically allocated.
 */
char *string_mime_type(emsMIMEtypeP mimePtr) 
{
	const char *kPrefixStr          = "Content-Type: ";
	const char *kParamSepStr        = ";\r\n  ";
	const char *kTypeSubtypeSepStr  = "/";
	const char *kAttValueSepStr     = "=";

	// Must have a valid mime structure
	if (!mimePtr)
		return NULL;

	// Both TYPE and SUBTYPE are required by RFC822
	if ((!mimePtr->type) || (!mimePtr->subType))
		return NULL;

	// Calculate the length of the header string
	unsigned int hLen = 0;

	hLen += strlen(kPrefixStr);
	hLen += rfc822_quoted_strlen(mimePtr->type);
	hLen += strlen(kTypeSubtypeSepStr);
	hLen += rfc822_quoted_strlen(mimePtr->subType);

	emsMIMEParamP paramPtr = mimePtr->params;

	while (paramPtr)
	{
		hLen += strlen(kParamSepStr);
		hLen += rfc822_quoted_strlen(paramPtr->name);
		hLen += strlen(kAttValueSepStr);
		hLen += rfc822_quoted_strlen(paramPtr->value);

		paramPtr = paramPtr->next;
	}

	// Allocate space for the header line
	char *hStr = (char *) malloc(hLen + 1);
	
	if (!hStr)
		return NULL;

	// Build the header line
	char *cp = hStr;

	cp = strchr(strcpy(cp, kPrefixStr), '\0');
	cp = rfc822_quote_strcpy(cp, mimePtr->type);
	cp = strchr(strcpy(cp, kTypeSubtypeSepStr), '\0');
	cp = rfc822_quote_strcpy(cp, mimePtr->subType);

	paramPtr = mimePtr->params;
	while (paramPtr)
	{
		cp = strchr(strcpy(cp, kParamSepStr), '\0');
		cp = rfc822_quote_strcpy(cp, paramPtr->name);
		cp = strchr(strcpy(cp, kAttValueSepStr), '\0');
		cp = rfc822_quote_strcpy(cp, paramPtr->value);

		paramPtr = paramPtr->next;
	}

	// Return the header line
	return (hStr);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Pick out a specific parameter name-value pair from an emsMIMEtype
 *  structure. The match is case sensitive.
 *
 *  NOTE: The user of this function should NOT alter the returned string
 *        in any way -- it should be considered READ-ONLY.
 *
 *  Args:
 *   mimePtr   [IN] Pointer to the emsMIMEtype structure
 *   paramName [IN] Name of parameter to look for
 *
 *  Returns: String of the associated value or NULL if no match is found
 */
const char *get_mime_parameter(emsMIMEtypeP mimePtr, const char *paramName)
{
	if (!mimePtr)
		return NULL;

	emsMIMEParamP paramPtr = mimePtr->params;

	while (paramPtr)
	{
		if (strcmp(paramPtr->name, paramName) == 0)
			return (paramPtr->value);

		paramPtr = paramPtr->next;
	}

    return (NULL);
}				

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Remove a parameter from an existing emsMIMEtype structure. This structure
 *  should be created using make_mime_type().
 *
 *  NOTE: All input strings are COPIED before permanent use.
 *
 * Args:
 *   mimePtr [IN] Pointer to the emsMIMEtype structure to altered
 *   name    [IN] Name of the parameter to be removed
 *   
 *  Returns: Boolean (TRUE = success, FALSE = failure)
 */
int remove_mime_parameter(emsMIMEtypeP mimePtr, const char *name)
{
	if (!mimePtr)
		return (FALSE);

	emsMIMEParamP paramPtr = mimePtr->params, prevParamPtr = NULL;

	/* Find the parameter */
	while (paramPtr)
	{
		if (strcmp(paramPtr->name, name) == 0)
			break;

		prevParamPtr = paramPtr;
		paramPtr = paramPtr->next;
	}

	if (!paramPtr)
	    return (FALSE); /* Not found */

	if (prevParamPtr == NULL) /* Removing first in list */
		mimePtr->params = paramPtr->next;
	else
		prevParamPtr->next = paramPtr->next;

	paramPtr->next = NULL;
	free_param_type(paramPtr);

	return (TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 *  Check for a matching MIME type/subtype. If either type or subtype is
 *  NULL, then it won't be checked. If both are NULL, then TRUE is always
 *  returned.
 *
 *  Args:
 *   mimePtr   [IN] Pointer to the emsMIMEtype structure
 *   mime_type [IN] The major MIME type to check
 *   sub_type  [IN] The MIME subtype to check
 *  
 *  Returns: Boolean (TRUE if the MIME type matches, FALSE if not)
 */
int match_mime_type(emsMIMEtypeP mimePtr, const char *mime_type, const char *sub_type)
{
	if (!mimePtr)
		return (FALSE);

	return (
			((mime_type == NULL) || (stricmp(mimePtr->type, mime_type) == 0))
			&&
			((sub_type == NULL) || (stricmp(mimePtr->subType, sub_type) == 0))
		   );
}

/* ========================================================================== */
/*                              LOCAL FUNCTIONS                               */
/* ========================================================================== */

/* Private function used to free the parameter linked-list */
/* static */ void free_param_type(emsMIMEParamP paramPtr)
{
	emsMIMEParamP nextParamPtr;

	while (paramPtr)
	{
		nextParamPtr = paramPtr->next;

		safefree(paramPtr->name);
		safefree(paramPtr->value);
		safefree(paramPtr);

		paramPtr = nextParamPtr;
	}
}

