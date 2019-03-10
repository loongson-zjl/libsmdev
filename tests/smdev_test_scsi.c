/*
 * Library scsi functions test program
 *
 * Copyright (C) 2010-2019, Joachim Metz <joachim.metz@gmail.com>
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
#include <file_stream.h>
#include <types.h>

#if defined( HAVE_STDLIB_H ) || defined( WINAPI )
#include <stdlib.h>
#endif

#if defined( HAVE_CYGWIN_HDREG_H )
#include <cygwin/hdreg.h>
#endif

#if defined( HAVE_LINUX_HDREG_H )
#include <linux/hdreg.h>
#endif

#include "smdev_test_libcerror.h"
#include "smdev_test_libcfile.h"
#include "smdev_test_libsmdev.h"
#include "smdev_test_macros.h"
#include "smdev_test_unused.h"

#include "../libsmdev/libsmdev_scsi.h"

#if defined( __GNUC__ ) && !defined( LIBSMDEV_DLL_IMPORT )

#if defined( HAVE_SCSI_SG_H )

/* Tests the libsmdev_scsi_get_bus_type function
 * Returns 1 if successful or 0 if not
 */
int smdev_test_scsi_get_bus_type(
     void )
{
	libcerror_error_t *error     = NULL;
	libcfile_file_t *device_file = NULL;
	uint8_t bus_type             = 0;
	int result                   = 0;

	/* Initialize test
	 */
	result = libcfile_file_initialize(
	          &device_file,
	          &error );

	SMDEV_TEST_ASSERT_EQUAL_INT(
	 "result",
	 result,
	 1 );

	SMDEV_TEST_ASSERT_IS_NOT_NULL(
	 "device_file",
	 device_file );

	SMDEV_TEST_ASSERT_IS_NULL(
	 "error",
	 error );

	/* Test regular cases
	 */
#if defined( WINAPI )
	result = libcfile_file_open_wide(
	          device_file,
	          L"\\\\.\\PhysicalDrive0",
	          LIBCFILE_OPEN_READ,
	          NULL );
#else
	result = libcfile_file_open(
	          device_file,
	          "/dev/sda",
	          LIBCFILE_OPEN_READ,
	          NULL );
#endif

	if( result == 1 )
	{
		result = libsmdev_scsi_get_bus_type(
		          device_file,
		          &bus_type,
		          &error );

		SMDEV_TEST_ASSERT_EQUAL_INT(
		 "result",
		 result,
		 1 );

		SMDEV_TEST_ASSERT_IS_NULL(
		 "error",
		 error );

		result = libcfile_file_close(
		          device_file,
		          &error );

		SMDEV_TEST_ASSERT_EQUAL_INT(
		 "result",
		 result,
		 0 );

		SMDEV_TEST_ASSERT_IS_NULL(
		 "error",
		 error );
	}
	/* Test on a closed file
	 */
	result = libsmdev_scsi_get_bus_type(
	          device_file,
	          &bus_type,
	          &error );

	SMDEV_TEST_ASSERT_EQUAL_INT(
	 "result",
	 result,
	 0 );

	SMDEV_TEST_ASSERT_IS_NULL(
	 "error",
	 error );

	/* Test error cases
	 */
	result = libsmdev_scsi_get_bus_type(
	          NULL,
	          &bus_type,
	          &error );

	SMDEV_TEST_ASSERT_EQUAL_INT(
	 "result",
	 result,
	 -1 );

	SMDEV_TEST_ASSERT_IS_NOT_NULL(
	 "error",
	 error );

	libcerror_error_free(
	 &error );

	result = libsmdev_scsi_get_bus_type(
	          device_file,
	          NULL,
	          &error );

	SMDEV_TEST_ASSERT_EQUAL_INT(
	 "result",
	 result,
	 -1 );

	SMDEV_TEST_ASSERT_IS_NOT_NULL(
	 "error",
	 error );

	libcerror_error_free(
	 &error );

	/* Clean up
	 */
	result = libcfile_file_free(
	          &device_file,
	          &error );

	SMDEV_TEST_ASSERT_EQUAL_INT(
	 "result",
	 result,
	 1 );

	SMDEV_TEST_ASSERT_IS_NULL(
	 "device_file",
	 device_file );

	SMDEV_TEST_ASSERT_IS_NULL(
	 "error",
	 error );

	return( 1 );

on_error:
	if( error != NULL )
	{
		libcerror_error_free(
		 &error );
	}
	if( device_file != NULL )
	{
		libcfile_file_free(
		 &device_file,
		 NULL );
	}
	return( 0 );
}

#endif /* defined( HAVE_SCSI_SG_H ) */

#endif /* defined( __GNUC__ ) && !defined( LIBSMDEV_DLL_IMPORT ) */

/* The main program
 */
#if defined( HAVE_WIDE_SYSTEM_CHARACTER )
int wmain(
     int argc SMDEV_TEST_ATTRIBUTE_UNUSED,
     wchar_t * const argv[] SMDEV_TEST_ATTRIBUTE_UNUSED )
#else
int main(
     int argc SMDEV_TEST_ATTRIBUTE_UNUSED,
     char * const argv[] SMDEV_TEST_ATTRIBUTE_UNUSED )
#endif
{
	SMDEV_TEST_UNREFERENCED_PARAMETER( argc )
	SMDEV_TEST_UNREFERENCED_PARAMETER( argv )

#if defined( __GNUC__ ) && !defined( LIBSMDEV_DLL_IMPORT )

#if defined( HAVE_SCSI_SG_H )

	/* TODO add tests for libsmdev_scsi_command */

	/* TODO add tests for libsmdev_scsi_ioctrl */

	/* TODO add tests for libsmdev_scsi_inquiry */

	/* TODO add tests for libsmdev_scsi_read_toc */

	/* TODO add tests for libsmdev_scsi_read_disc_information */

	/* TODO add tests for libsmdev_scsi_read_track_information */

	/* TODO add tests for libsmdev_scsi_get_identifier */

	SMDEV_TEST_RUN(
	 "libsmdev_scsi_get_bus_type",
	 smdev_test_scsi_get_bus_type );

	/* TODO add tests for libsmdev_scsi_get_pci_bus_address */

#endif /* defined( HAVE_SCSI_SG_H ) */

#endif /* defined( __GNUC__ ) && !defined( LIBSMDEV_DLL_IMPORT ) */

	return( EXIT_SUCCESS );

on_error:
	return( EXIT_FAILURE );
}

