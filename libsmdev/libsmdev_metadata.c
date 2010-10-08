/*
 * Meta data functions
 *
 * Copyright (c) 2010, Joachim Metz <jbmetz@users.sourceforge.net>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <memory.h>
#include <types.h>

#include <libcstring.h>
#include <liberror.h>
#include <libnotify.h>

#if defined( HAVE_SYS_IOCTL_H )
#include <sys/ioctl.h>
#endif

#if defined( WINAPI )
#include <io.h>
#include <winioctl.h>

#elif defined( HAVE_CYGWIN_FS_H )
#include <cygwin/fs.h>

#elif defined( HAVE_LINUX_FS_H )
/* Required for Linux platforms that use a sizeof( u64 )
 * in linux/fs.h but have no typedef of it
 */
#if !defined( HAVE_U64 )
typedef size_t u64;
#endif

#include <linux/fs.h>

#else

#if defined( HAVE_SYS_DISK_H )
#include <sys/disk.h>
#endif

#if defined( HAVE_SYS_DISKLABEL_H )
#include <sys/disklabel.h>
#endif

#endif

#include "libsmdev_ata.h"
#include "libsmdev_definitions.h"
#include "libsmdev_handle.h"
#include "libsmdev_libuna.h"
#include "libsmdev_offset_list.h"
#include "libsmdev_optical_disk.h"
#include "libsmdev_scsi.h"
#include "libsmdev_string.h"
#include "libsmdev_types.h"

#if defined( WINAPI )

#if !defined( IOCTL_DISK_GET_LENGTH_INFO )
#define IOCTL_DISK_GET_LENGTH_INFO \
	CTL_CODE( IOCTL_DISK_BASE, 0x0017, METHOD_BUFFERED, FILE_READ_ACCESS )

typedef struct
{
	LARGE_INTEGER Length;
}
GET_LENGTH_INFORMATION;

#endif /* !defined( IOCTL_DISK_GET_LENGTH_INFO ) */

#if !defined( IOCTL_DISK_GET_DRIVE_GEOMETRY_EX )
#define IOCTL_DISK_GET_DRIVE_GEOMETRY_EX \
	CTL_CODE( IOCTL_DISK_BASE, 0x0028, METHOD_BUFFERED, FILE_ANY_ACCESS )

typedef struct _DISK_GEOMETRY_EX
{
	DISK_GEOMETRY Geometry;
	LARGE_INTEGER DiskSize;
	UCHAR Data[ 1 ];
}
DISK_GEOMETRY_EX, *PDISK_GEOMETRY_EX;

#endif /* !defined( IOCTL_DISK_GET_DRIVE_GEOMETRY_EX ) */

#if !defined( IOCTL_STORAGE_QUERY_PROPERTY )
#define IOCTL_STORAGE_QUERY_PROPERTY \
	CTL_CODE( IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS )

typedef enum _STORAGE_PROPERTY_ID
{
	StorageDeviceProperty = 0,
	StorageAdapterProperty,
	StorageDeviceIdProperty,
	StorageDeviceUniqueIdProperty,
	StorageDeviceWriteCacheProperty,
	StorageMiniportProperty,
	StorageAccessAlignmentProperty,
	StorageDeviceSeekPenaltyProperty,
	StorageDeviceTrimProperty,
	StorageDeviceWriteAggregationProperty
}
STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef enum _STORAGE_QUERY_TYPE
{
	PropertyStandardQuery = 0, 
	PropertyExistsQuery, 
	PropertyMaskQuery, 
	PropertyQueryMaxDefined 
}
STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

#if defined( __MINGW32_VERSION )
typedef enum _STORAGE_BUS_TYPE
{
	BusTypeUnknown		= 0x00,
	BusTypeScsi		= 0x01,
	BusTypeAtapi		= 0x02,
	BusTypeAta		= 0x03,
	BusType1394		= 0x04,
	BusTypeSsa		= 0x05,
	BusTypeFibre		= 0x06,
	BusTypeUsb		= 0x07,
	BusTypeRAID		= 0x08,
	BusTypeiSCSI		= 0x09,
	BusTypeSas		= 0x0a,
	BusTypeSata		= 0x0b,
	BusTypeMaxReserved	= 0x7f 
}
STORAGE_BUS_TYPE, *PSTORAGE_BUS_TYPE;
#endif

typedef struct _STORAGE_PROPERTY_QUERY
{
	STORAGE_PROPERTY_ID PropertyId;
	STORAGE_QUERY_TYPE QueryType;
	UCHAR AdditionalParameters[ 1 ];
}
STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;

typedef struct _STORAGE_DEVICE_DESCRIPTOR
{
	ULONG Version;
	ULONG Size;
	UCHAR DeviceType;
	UCHAR DeviceTypeModifier;
	BOOLEAN RemovableMedia;
	BOOLEAN CommandQueueing;
	ULONG VendorIdOffset;
	ULONG ProductIdOffset;
	ULONG ProductRevisionOffset;
	ULONG SerialNumberOffset;
	STORAGE_BUS_TYPE BusType;
	ULONG RawPropertiesLength;
	UCHAR RawDeviceProperties[ 1 ];
}
STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;

typedef struct _STORAGE_DESCRIPTOR_HEADER
{
	ULONG Version;
	ULONG Size;
}
STORAGE_DESCRIPTOR_HEADER, *PSTORAGE_DESCRIPTOR_HEADER;

#endif /* !defined( IOCTL_STORAGE_QUERY_PROPERTY ) */

#endif /* defined( WINAPI ) */

/* Retrieves the media size
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_get_media_size(
     libsmdev_handle_t *handle,
     size64_t *media_size,
     liberror_error_t **error )
{
#if defined( WINAPI )
	GET_LENGTH_INFORMATION length_information;

	DWORD response_count                        = 0;
#else
#if !defined( DIOCGMEDIASIZE ) && defined( DIOCGDINFO )
	struct disklabel disk_label;
#endif
#if defined( DKIOCGETBLOCKCOUNT )
	uint64_t block_count                        = 0;
#endif
#endif

	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_media_size";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

#if defined( WINAPI )
	if( internal_handle->file_handle == INVALID_HANDLE_VALUE )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing file handle.",
		 function );

		return( -1 );
	}
#else
	if( internal_handle->file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid handle - missing file descriptor.",
		 function );

		return( -1 );
	}
#endif
	if( media_size == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid media size.",
		 function );

		return( -1 );
	}
	if( internal_handle->media_size_set == 0 )
	{
#if defined( WINAPI )
		if( DeviceIoControl(
		     internal_handle->file_handle,
		     IOCTL_DISK_GET_LENGTH_INFO,
		     NULL,
		     0,
		     &length_information,
		     sizeof( GET_LENGTH_INFORMATION ),
		     &response_count,
		     NULL ) == 0 )
		{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_IO,
				 LIBERROR_IO_ERROR_IOCTL_FAILED,
				 "%s: unable to query device for: IOCTL_DISK_GET_LENGTH_INFO.",
				 function );

				return( -1 );
		}
		internal_handle->media_size     = ( (size64_t) length_information.Length.HighPart << 32 )
		                                + length_information.Length.LowPart;
		internal_handle->media_size_set = 1;

#elif defined( BLKGETSIZE64 )
		if( ioctl(
		     internal_handle->file_descriptor,
		     BLKGETSIZE64,
		     &( internal_handle->media_size ) ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: BLKGETSIZE64.",
			 function );

			return( -1 );
		}
		internal_handle->media_size_set = 1;

#elif defined( DIOCGMEDIASIZE )
		if( ioctl(
		     internal_handle->file_descriptor,
		     DIOCGMEDIASIZE,
		     &( internal_handle->media_size ) ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: DIOCGMEDIASIZE.",
			 function );

			return( -1 );
		}
		internal_handle->media_size_set = 1;

#elif defined( DIOCGDINFO )
		if( ioctl(
		     internal_handle->file_descriptor,
		     DIOCGDINFO,
		     &disk_label ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: DIOCGDINFO.",
			 function );

			return( -1 );
		}
		internal_handle->media_size     = disk_label.d_secperunit * disk_label.d_secsize;
		internal_handle->media_size_set = 1;

#elif defined( DKIOCGETBLOCKCOUNT )
		if( internal_handle->bytes_per_sector_set == 0 )
		{
			if( ioctl(
			     internal_handle->file_descriptor,
			     DKIOCGETBLOCKSIZE,
			     &( internal_handle->bytes_per_sector ) ) == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_IO,
				 LIBERROR_IO_ERROR_IOCTL_FAILED,
				 "%s: unable to query device for: DKIOCGETBLOCKSIZE.",
				 function );

				return( -1 );
			}
			internal_handle->bytes_per_sector_set = 1;
		}
		if( ioctl(
		     internal_handle->file_descriptor,
		     DKIOCGETBLOCKCOUNT,
		     &block_count ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: DKIOCGETBLOCKCOUNT.",
			 function );

			return( -1 );
		}
		internal_handle->media_size     = (size64_t) block_count * (size64_t) internal_handle->bytes_per_sector;
		internal_handle->media_size_set = 1;

#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "%s: block size: %" PRIu32 " block count: %" PRIu64 " ",
			 function,
			 internal_handle->bytes_per_sector,
			 block_count );
		}
#endif
#endif
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "%s: media size: %" PRIu64 "\n",
			 function,
			 internal_handle->media_size );
		}
#endif
	}
	if( internal_handle->media_size_set == 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported platform.",
		 function );

		return( -1 );
	}
	*media_size = internal_handle->media_size;

	return( 1 );
}

/* Retrieves the number of bytes per sector
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_get_bytes_per_sector(
     libsmdev_handle_t *handle,
     uint32_t *bytes_per_sector,
     liberror_error_t **error )
{
#if defined( WINAPI )
	DISK_GEOMETRY_EX disk_geometry;

	DWORD response_count                        = 0;
#endif

	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_bytes_per_sector";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

#if defined( WINAPI )
	if( internal_handle->file_handle == INVALID_HANDLE_VALUE )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file handle.",
		 function );

		return( -1 );
	}
#else
	if( internal_handle->file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file descriptor.",
		 function );

		return( -1 );
	}
#endif
	if( bytes_per_sector == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid bytes per sector.",
		 function );

		return( -1 );
	}
	if( internal_handle->bytes_per_sector_set == 0 )
	{
#if defined( WINAPI )
		if( DeviceIoControl(
		     internal_handle->file_handle,
		     IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
		     NULL,
		     0,
		     &disk_geometry,
		     sizeof( DISK_GEOMETRY_EX ),
		     &response_count,
		     NULL ) == 0 )
		{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_IO,
				 LIBERROR_IO_ERROR_IOCTL_FAILED,
				 "%s: unable to query device for: IOCTL_DISK_GET_DRIVE_GEOMETRY_EX.",
				 function );

				return( -1 );
		}
		internal_handle->bytes_per_sector     = (uint32_t) disk_geometry.Geometry.BytesPerSector; 
		internal_handle->bytes_per_sector_set = 1;

#elif defined( BLKSSZGET )
		if( ioctl(
		     internal_handle->file_descriptor,
		     BLKSSZGET,
		     &( internal_handle->bytes_per_sector ) ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: BLKSSZGET.",
			 function );

			return( -1 );
		}
		internal_handle->bytes_per_sector_set = 1;

#elif defined( DKIOCGETBLOCKCOUNT )
		if( ioctl(
		     internal_handle->file_descriptor,
		     DKIOCGETBLOCKSIZE,
		     &( internal_handle->bytes_per_sector ) ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: DKIOCGETBLOCKSIZE.",
			 function );

			return( -1 );
		}
		internal_handle->bytes_per_sector_set = 1;
#endif
	}
	if( internal_handle->bytes_per_sector_set == 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported platform.",
		 function );

		return( -1 );
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libnotify_verbose != 0 )
	{
		libnotify_printf(
		 "%s: sector size: %" PRIu32 "\n",
		 function,
		 internal_handle->bytes_per_sector );
	}
#endif

	*bytes_per_sector = internal_handle->bytes_per_sector;

	return( 1 );
}

/* Determines the media information
 * Returns 1 if successful, 0 if no media information available or -1 on error
 */
int libsmdev_internal_handle_determine_media_information(
     libsmdev_internal_handle_t *internal_handle,
     liberror_error_t **error )
{
#if defined( WINAPI )
	STORAGE_PROPERTY_QUERY query;

	uint8_t *response      = NULL;
	size_t response_size   = 1024;
	size_t string_length   = 0;
	DWORD response_count   = 0;

#else
#if defined( HAVE_SCSI_SG_H )
	uint8_t response[ 255 ];
	ssize_t response_count = 0;
#endif
#if defined( HDIO_GET_IDENTITY )
	struct hd_driveid device_configuration;
#endif
#endif

	static char *function  = "libsmdev_internal_handle_determine_media_information";
	ssize_t result         = 0;

	if( internal_handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid device handle.",
		 function );

		return( -1 );
	}
	if( internal_handle->media_information_set != 0 )
	{
		return( 1 );
	}
#if defined( WINAPI )
	if( internal_handle->file_handle == INVALID_HANDLE_VALUE )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file handle.",
		 function );

		return( -1 );
	}
	if( memset(
	     &query,
	     0,
	     sizeof( STORAGE_PROPERTY_QUERY ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear storage property query.",
		 function );

		return( -1 );
	}
	query.PropertyId = StorageDeviceProperty;
	query.QueryType  = PropertyStandardQuery;

	response         = (uint8_t *) memory_allocate(
					response_size );

	if( response == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to response.",
		 function );

		return( -1 );
	}
	if( DeviceIoControl(
	     internal_handle->file_handle,
	     IOCTL_STORAGE_QUERY_PROPERTY,
	     &query,
	     sizeof( STORAGE_PROPERTY_QUERY ),
	     response,
	     response_size,
	     &response_count,
	     NULL ) == 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_IO,
		 LIBERROR_IO_ERROR_IOCTL_FAILED,
		 "%s: unable to query device for: IOCTL_STORAGE_QUERY_PROPERTY.",
		 function );

		memory_free(
		 response );

		return( -1 );
	}
	if( (size_t) ( (STORAGE_DESCRIPTOR_HEADER *) response )->Size > response_size )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_OUT_OF_RANGE,
		 "%s: response buffer too small.\n",
		 function );

		memory_free(
		 response );

		return( -1 );
	}
	if( (size_t) ( (STORAGE_DESCRIPTOR_HEADER *) response )->Size > sizeof( STORAGE_DEVICE_DESCRIPTOR ) )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_print_data(
			 response,
			 (size_t) response_count );
		}
#endif

		if( ( (STORAGE_DEVICE_DESCRIPTOR *) response )->VendorIdOffset > 0 )
		{
			string_length = libcstring_narrow_string_length(
					 (char *) &( response[ ( (STORAGE_DEVICE_DESCRIPTOR *) response )->VendorIdOffset ] ) );

			result = libsmdev_string_trim_copy_from_byte_stream(
				  internal_handle->vendor,
				  64,
				  &( response[ ( (STORAGE_DEVICE_DESCRIPTOR *) response )->VendorIdOffset ] ),
				  string_length,
				  error );

			if( result == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_SET_FAILED,
				 "%s: unable to set vendor.",
				 function );

				return( -1 );
			}
		}
		if( ( (STORAGE_DEVICE_DESCRIPTOR *) response )->ProductIdOffset > 0 )
		{
			string_length = libcstring_narrow_string_length(
					 (char *) &( response[ ( (STORAGE_DEVICE_DESCRIPTOR *) response )->ProductIdOffset ] ) );

			result = libsmdev_string_trim_copy_from_byte_stream(
				  internal_handle->model,
				  64,
				  &( response[ ( (STORAGE_DEVICE_DESCRIPTOR *) response )->ProductIdOffset ] ),
				  string_length,
				  error );

			if( result == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_SET_FAILED,
				 "%s: unable to set model.",
				 function );

				return( -1 );
			}
		}
		if( ( (STORAGE_DEVICE_DESCRIPTOR *) response )->SerialNumberOffset > 0 )
		{
			string_length = libcstring_narrow_string_length(
					 (char *) &( response[ ( (STORAGE_DEVICE_DESCRIPTOR *) response )->SerialNumberOffset ] ) );

			result = libsmdev_string_trim_copy_from_byte_stream(
				  internal_handle->serial_number,
				  64,
				  &( response[ ( (STORAGE_DEVICE_DESCRIPTOR *) response )->SerialNumberOffset ] ),
				  string_length,
				  error );

			if( result == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_SET_FAILED,
				 "%s: unable to set serial number.",
				 function );

				return( -1 );
			}
		}
		internal_handle->removable = ( (STORAGE_DEVICE_DESCRIPTOR *) response )->RemovableMedia;

		switch( ( ( STORAGE_DEVICE_DESCRIPTOR *) response )->BusType )
		{
			case BusTypeScsi:
				internal_handle->bus_type = LIBSMDEV_BUS_TYPE_SCSI;
				break;

			case BusTypeAtapi:
			case BusTypeAta:
				internal_handle->bus_type = LIBSMDEV_BUS_TYPE_ATA;
				break;

			case BusType1394:
				internal_handle->bus_type = LIBSMDEV_BUS_TYPE_FIREWIRE;
				break;

			case BusTypeUsb:
				internal_handle->bus_type = LIBSMDEV_BUS_TYPE_USB;
				break;

			default:
				break;
		}
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "Bus type:\t\t" );

			switch( ( ( STORAGE_DEVICE_DESCRIPTOR *) response )->BusType )
			{
				case BusTypeScsi:
					libnotify_printf(
					 "SCSI" );
					break;

				case BusTypeAtapi:
					libnotify_printf(
					 "ATAPI" );
					break;

				case BusTypeAta:
					libnotify_printf(
					 "ATA" );
					break;

				case BusType1394:
					libnotify_printf(
					 "FireWire (IEEE1394)" );
					break;

				case BusTypeSsa:
					libnotify_printf(
					 "Serial Storage Architecture (SSA)" );
					break;

				case BusTypeFibre:
					libnotify_printf(
					 "Fibre Channel" );
					break;

				case BusTypeUsb:
					libnotify_printf(
					 "USB" );
					break;

				case BusTypeRAID:
					libnotify_printf(
					 "RAID" );
					break;

				case BusTypeiScsi:
					libnotify_printf(
					 "iSCSI" );
					break;

				case BusTypeSas:
					libnotify_printf(
					 "SAS" );
					break;

				case BusTypeSata:
					libnotify_printf(
					 "SATA" );
					break;

				case BusTypeSd:
					libnotify_printf(
					 "Secure Digital (SD)" );
					break;

				case BusTypeMmc:
					libnotify_printf(
					 "Multi Media Card (MMC)" );
					break;

				default:
					libnotify_printf(
					 "Unknown: %d",
					 ( ( STORAGE_DEVICE_DESCRIPTOR *) response )->BusType );
					break;
			}
			libnotify_printf(
			 "\n" );
		}
#endif
	}
	memory_free(
	 response );
#else
	if( internal_handle->file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file descriptor.",
		 function );

		return( -1 );
	}
#if defined( HAVE_SCSI_SG_H )
	/* Use the Linux sg (generic SCSI) driver to determine device information
	 */
	if( libsmdev_scsi_get_bus_type(
	     internal_handle->file_descriptor,
	     &( internal_handle->bus_type ),
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to determine bus type.",
		 function );

		return( -1 );
	}
	response_count = libsmdev_scsi_inquiry(
			  internal_handle->file_descriptor,
			  0x00,
			  0x00,
			  response,
			  255,
			  NULL );

/* TODO what to do about garbarge return ? */

	if( response_count >= 5 )
	{
		internal_handle->removable   = ( response[ 1 ] & 0x80 ) >> 7;
		internal_handle->device_type = ( response[ 0 ] & 0x1f );

#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "%s: removable\t\t: %" PRIu8 "\n",
			 function,
			 internal_handle->removable );

			libnotify_printf(
			 "%s: device type\t: 0x%" PRIx8 "\n",
			 function,
			 internal_handle->device_type );

			libnotify_printf(
			 "\n" );
		}
#endif
	}
	if( response_count >= 16 )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_print_data(
			 response,
			 response_count );
		}
#endif
		result = libsmdev_string_trim_copy_from_byte_stream(
			  internal_handle->vendor,
			  64,
			  &( response[ 8 ] ),
			  15 - 8,
			  error );

		if( result == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set vendor.",
			 function );

			return( -1 );
		}
	}
	if( response_count >= 32 )
	{
		result = libsmdev_string_trim_copy_from_byte_stream(
			  internal_handle->model,
			  64,
			  &( response[ 16 ] ),
			  31 - 16,
			  error );

		if( result == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set model.",
			 function );

			return( -1 );
		}
	}
	response_count = libsmdev_scsi_inquiry(
			  internal_handle->file_descriptor,
			  0x01,
			  0x80,
			  response,
			  255,
			  NULL );

	if( response_count > 4 )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_print_data(
			 response,
			 response_count );
		}
#endif
		result = libsmdev_string_trim_copy_from_byte_stream(
			  internal_handle->serial_number,
			  64,
			  &( response[ 4 ] ),
			  response_count - 4,
			  error );

		if( result == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set serial number.",
			 function );

			return( -1 );
		}
	}
#endif
#if defined( HDIO_GET_IDENTITY )
	if( internal_handle->bus_type == LIBSMDEV_BUS_TYPE_ATA )
	{
		if( libsmdev_ata_get_device_configuration(
		     internal_handle->file_descriptor,
		     &device_configuration,
		     error ) == -1 )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( ( error != NULL )
			 && ( *error != NULL ) )
			{
				libnotify_print_error_backtrace(
				 *error );
			}
#endif
			liberror_error_free(
			 error );
		}
		else
		{
			result = libsmdev_string_trim_copy_from_byte_stream(
				  internal_handle->serial_number,
				  64,
				  device_configuration.serial_no,
				  20,
				  error );

			if( result == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_SET_FAILED,
				 "%s: unable to set serial number.",
				 function );

				return( -1 );
			}
			result = libsmdev_string_trim_copy_from_byte_stream(
				  internal_handle->model,
				  64,
				  device_configuration.model,
				  40,
				  error );

			if( result == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_SET_FAILED,
				 "%s: unable to set model.",
				 function );

				return( -1 );
			}
			internal_handle->removable   = ( device_configuration.config & 0x0080 ) >> 7;
			internal_handle->device_type = ( device_configuration.config & 0x1f00 ) >> 8;

#if defined( HAVE_DEBUG_OUTPUT )
			if( libnotify_verbose != 0 )
			{
				libnotify_printf(
				 "%s: removable\t\t: %" PRIu8 "\n",
				 function,
				 internal_handle->removable );

				libnotify_printf(
				 "%s: device type\t: 0x%" PRIx8 "\n",
				 function,
				 internal_handle->device_type );

				libnotify_printf(
				 "\n" );
			}
#endif
		}
	}
#endif
#if defined( HAVE_LINUX_CDROM_H )
	if( internal_handle->device_type == 0x05 )
	{
/* TODO */
		if( libsmdev_optical_disk_get_table_of_contents(
		     internal_handle->file_descriptor,
		     NULL,
		     error ) != 1 )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( ( error != NULL )
			 && ( *error != NULL ) )
			{
				libnotify_print_error_backtrace(
				 *error );
			}
#endif
			liberror_error_free(
			 error );
		}
	}
#endif
/* Disabled for now
	if( libsmdev_scsi_get_identier(
	     internal_handle->file_descriptor,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to determine SCSI identifier.",
		 function );

		return( -1 );
	}
	uint8_t pci_bus_address[ 64 ];
	size_t pci_bus_address_size = 64;

	if( libsmdev_scsi_get_pci_bus_address(
	     internal_handle->file_descriptor,
	     pci_bus_address,
	     pci_bus_address_size,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to determine PCI bus address.",
		 function );

		return( -1 );
	}
#if defined( HAVE_LINUX_USB_CH9_H )
	if( internal_handle->bus_type == LIBSMDEV_BUS_TYPE_USB )
	{
		if( io_usb_test(
		     internal_handle->file_descriptor,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_GENERIC,
			 "%s: unable to test USB.",
			 function );

			return( -1 );
		}
	}
#endif
*/
#endif
	internal_handle->media_information_set = 1;

	return( 1 );
}

/* Retrieves the media type
 * Returns 1 if successful or -1 on error
 */
int libsmdev_handle_get_media_type(
     libsmdev_handle_t *handle,
     uint8_t *media_type,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_media_type";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

#if defined( WINAPI )
	if( internal_handle->file_handle == INVALID_HANDLE_VALUE )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file handle.",
		 function );

		return( -1 );
	}
#else
	if( internal_handle->file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file descriptor.",
		 function );

		return( -1 );
	}
#endif
	if( media_type == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid media type.",
		 function );

		return( -1 );
	}
	if( internal_handle->media_information_set == 0 )
	{
		if( libsmdev_internal_handle_determine_media_information(
		     internal_handle,
		     error ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to determine media information.",
			 function );

			return( -1 );
		}
	}
	if( internal_handle->device_type == 0x05 )
	{
		*media_type = LIBSMDEV_MEDIA_TYPE_OPTICAL;
	}
	else if( internal_handle->removable != 0 )
	{
		*media_type = LIBSMDEV_MEDIA_TYPE_REMOVABLE;
	}
	else
	{
		*media_type = LIBSMDEV_MEDIA_TYPE_FIXED;
	}
	return( 1 );
}

/* Retrieves the bus type
 * Returns 1 if successful or -1 on error
 */
int libsmdev_handle_get_bus_type(
     libsmdev_handle_t *handle,
     uint8_t *bus_type,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_bus_type";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

#if defined( WINAPI )
	if( internal_handle->file_handle == INVALID_HANDLE_VALUE )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file handle.",
		 function );

		return( -1 );
	}
#else
	if( internal_handle->file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid device handle - missing file descriptor.",
		 function );

		return( -1 );
	}
#endif
	if( bus_type == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid bus type.",
		 function );

		return( -1 );
	}
	if( internal_handle->media_information_set == 0 )
	{
		if( libsmdev_internal_handle_determine_media_information(
		     internal_handle,
		     error ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to determine media information.",
			 function );

			return( -1 );
		}
	}
	*bus_type = internal_handle->bus_type;

	return( 1 );
}

/* Retrieves an information value specified by the identifier
 * The strings are encoded in UTF-8
 * The value size should include the end of string character
 * Returns 1 if successful, 0 if value not present or -1 on error
 */
int libsmdev_handle_get_information_value(
     libsmdev_handle_t *handle,
     const uint8_t *information_value_identifier,
     size_t information_value_identifier_length,
     uint8_t *information_value,
     size_t information_value_size,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	uint8_t *internal_information_value         = NULL;
	static char *function                       = "libsmdev_handle_get_information_value";
	size_t internal_information_value_size      = 0;

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( information_value_identifier == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid information value identifier.",
		 function );

		return( -1 );
	}
	if( information_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid information value.",
		 function );

		return( -1 );
	}
	if( internal_handle->media_information_set == 0 )
	{
		if( libsmdev_internal_handle_determine_media_information(
		     internal_handle,
		     error ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to determine media information.",
			 function );

			return( -1 );
		}
	}
	if( ( information_value_identifier_length == 5 )
	 && ( libcstring_narrow_string_compare(
	       "model",
	       (char *) information_value_identifier,
	       information_value_identifier_length ) == 0 ) )
	{
		internal_information_value = (uint8_t *) internal_handle->model;
	}
	else if( ( information_value_identifier_length == 6 )
	      && ( libcstring_narrow_string_compare(
		    "vendor",
		    (char *) information_value_identifier,
		    information_value_identifier_length ) == 0 ) )
	{
		internal_information_value = (uint8_t *) internal_handle->vendor;
	}
	else if( ( information_value_identifier_length == 13 )
	      && ( libcstring_narrow_string_compare(
		    "serial_number",
		    (char *) information_value_identifier,
		    information_value_identifier_length ) == 0 ) )
	{
		internal_information_value = (uint8_t *) internal_handle->serial_number;
	}
	else
	{
		return( 0 );
	}
	if( internal_information_value != NULL )
	{
		if( internal_information_value[ 0 ] == 0 )
		{
			return( 0 );
		}
		/* Determine the header value size
		 */
		internal_information_value_size = 1 + libcstring_string_length(
		                                       (char *) internal_information_value );

		if( information_value_size < internal_information_value_size )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
			 LIBERROR_ARGUMENT_ERROR_VALUE_TOO_SMALL,
			 "%s: information value too small.",
			 function );

			return( -1 );
		}
		if( libcstring_string_copy(
		     information_value,
		     internal_information_value,
		     internal_information_value_size ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set information value.",
			 function );

			return( -1 );
		}
		information_value[ internal_information_value_size - 1 ] = 0;
	}
	return( 1 );
}

/* Retrieves the number of sessions
 * Returns 1 if successful or -1 on error
 */
int libsmdev_handle_get_number_of_sessions(
     libsmdev_handle_t *handle,
     int *number_of_sessions,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_number_of_sessions";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( libsmdev_offset_list_get_number_of_elements(
	     internal_handle->sessions,
	     number_of_sessions,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of elements in sessions offset list.",
		 function );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves a session
 * Returns 1 if successful or -1 on error
 */
int libsmdev_handle_get_session(
     libsmdev_handle_t *handle,
     int index,
     off64_t *offset,
     size64_t *size,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_session";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( libsmdev_offset_list_get_offset(
	     internal_handle->sessions,
	     index,
	     offset,
	     size,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve session: %d from sessions offset list.",
		 function,
		 index );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves the number of read/write error retries
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_get_number_of_error_retries(
     libsmdev_handle_t *handle,
     uint8_t *number_of_error_retries,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_number_of_error_retries";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( number_of_error_retries == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of error retries.",
		 function );

		return( -1 );
	}
	*number_of_error_retries = internal_handle->number_of_error_retries;

	return( 1 );
}

/* Sets the number of read/write error retries
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_set_number_of_error_retries(
     libsmdev_handle_t *handle,
     uint8_t number_of_error_retries,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_set_number_of_error_retries";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	internal_handle->number_of_error_retries = number_of_error_retries;

	return( 1 );
}

/* Retrieves the read/write error granularity
 * A value of 0 represents an error granularity of the entire buffer being read/written
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_get_error_granularity(
     libsmdev_handle_t *handle,
     size_t *error_granularity,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_error_granularity";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( error_granularity == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid read granularity.",
		 function );

		return( -1 );
	}
	*error_granularity = internal_handle->error_granularity;

	return( 1 );
}

/* Sets the read/write error granularity
 * A value of 0 represents an error granularity of the entire buffer being read/written
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_set_error_granularity(
     libsmdev_handle_t *handle,
     size_t error_granularity,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_set_error_granularity";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( error_granularity > (size_t) SSIZE_MAX )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid error granularity value exceeds maximum.",
		 function );

		return( -1 );
	}
	internal_handle->error_granularity = error_granularity;

	return( 1 );
}

/* Retrieves the read/write error flags
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_get_error_flags(
     libsmdev_handle_t *handle,
     uint8_t *error_flags,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_error_flags";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( error_flags == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid error flags.",
		 function );

		return( -1 );
	}
	*error_flags = internal_handle->error_flags;

	return( 1 );
}

/* Sets the the read/write error flags
 * Returns the 1 if succesful or -1 on error
 */
int libsmdev_handle_set_error_flags(
     libsmdev_handle_t *handle,
     uint8_t error_flags,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_set_error_flags";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( ( error_flags & ~( LIBSMDEV_ERROR_FLAG_ZERO_ON_ERROR ) ) != 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported error flags.",
		 function );

		return( -1 );
	}
	internal_handle->error_flags = error_flags;

	return( 1 );
}

/* Retrieves the number of read/write errors
 * Returns 1 if successful or -1 on error
 */
int libsmdev_handle_get_number_of_errors(
     libsmdev_handle_t *handle,
     int *number_of_errors,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_number_of_errors";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( libsmdev_offset_list_get_number_of_elements(
	     internal_handle->errors,
	     number_of_errors,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of elements in errors offset list.",
		 function );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves a read/write error
 * Returns 1 if successful or -1 on error
 */
int libsmdev_handle_get_error(
     libsmdev_handle_t *handle,
     int index,
     off64_t *offset,
     size64_t *size,
     liberror_error_t **error )
{
	libsmdev_internal_handle_t *internal_handle = NULL;
	static char *function                       = "libsmdev_handle_get_error";

	if( handle == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid handle.",
		 function );

		return( -1 );
	}
	internal_handle = (libsmdev_internal_handle_t *) handle;

	if( libsmdev_offset_list_get_offset(
	     internal_handle->errors,
	     index,
	     offset,
	     size,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve error: %d from errors offset list.",
		 function,
		 index );

		return( -1 );
	}
	return( 1 );
}

