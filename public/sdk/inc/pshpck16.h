/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    pshpack8.h

Abstract:

    This file turns 8 byte packing of structures on.  (That is, it disables
    automatic alignment of structure fields.)  An include file is needed
    because various compilers do this in different ways.  For Microsoft
    compatible compilers, this files uses the push option to the pack pragma
    so that the poppack.h include file can restore the previous packing
    reliably.

    The file poppack.h is the complement to this file.

--*/

#if ! (defined(lint) || defined(RC_INVOKED))
#if ( _MSC_VER >= 800 ) || defined(_PUSHPOP_SUPPORTED)
#pragma warning(disable:4103)
#if !(defined( MIDL_PASS )) || defined( __midl )
#ifdef __clang__
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#pragma pack(push,16)
#else
#pragma pack(16)
#endif
#else
#pragma pack(16)
#endif
#endif // ! (defined(lint) || defined(RC_INVOKED))
