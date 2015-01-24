/***************************************************************************/
/*
 * Parses descriptors
 *
 * Copyright (c) 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 *
 */
/***************************************************************************/
/*
 * This file contains functions excerpted LIBUSB's usb.c & descriptors.c,
 * along with an OS/2-specific function modeled on code from linux.c.
 * All have been modified to suit the needs of Cameraderie and my own
 * coding style.  To the extent permitted by the LGPL, my changes and
 * additions are licensed under the Mozilla Public License v1.1
 * 
 * Copyright (c) 2006, 2007 Richard L Walsh <rich@e-vertise.com>
 * Copyright (c) 2015, bww bitwise works GmbH
 *
 * 2015-01-22 Silvan Scherrer enhanced UsbQueryDeviceReport to 4096 byte 
 *
 */
/***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <os2.h>
#include "usbcalls.h"
#include "camlibusb.h"

/***************************************************************************/

int     usb_find_devices( usb_device **devices);
int     usb_parse_configuration( usb_config_descriptor *config, char *buffer);
int     usb_parse_interface( usb_interface *interface,
                             unsigned char *buffer, int size);
int     usb_parse_endpoint( usb_endpoint_descriptor *endpoint,
                              unsigned char *buffer, int size);
void    usb_free_dev( usb_device *dev);
void    usb_destroy_configuration( usb_device *dev);

/***************************************************************************/

// for each device, UsbQueryDeviceReport() returns a device descriptor
// followed by a series of configuration descriptors which this parses

int         usb_find_devices( usb_device **devices)
{
  int       rtn = 0;
  int       rc;
  ULONG     cntDev;
  int       ctr;
  int       i;
  ULONG     len;
  usb_device *   devList = 0;
  usb_device *   dev;
  char      report[4096];

  rc = UsbQueryNumberDevices( &cntDev);
  if (rc) {
    printf( "UsbQueryNumberDevices - rc= 0x%x\n", (int)rc);
    return (rc);
  }

  for (ctr = 1; ctr <= cntDev; ctr++) {

    len = sizeof(report);
    rc = UsbQueryDeviceReport( ctr, &len, report);
    if (rc) {
      printf( "UsbQueryDeviceReport - device= %d  rc= 0x%x\n",
              (int)ctr, (int)rc);
      return (rc);
    }

    dev = calloc( 1, sizeof(usb_device));
    if (!dev) {
      printf( "malloc\n");
      return (-1);
    }

    dev->next = devList;
    devList = dev;

    dev->devnum = ctr;
    memcpy( &dev->descriptor, report, sizeof(dev->descriptor));

    printf( "Found device %d\n", dev->devnum);

    if (dev->descriptor.bNumConfigurations > USB_MAXCONFIG ||
        dev->descriptor.bNumConfigurations < 1) {
      printf( "invalid number of device configurations - nbr= %d  continuing...\n",
                     dev->descriptor.bNumConfigurations);
      continue;
    }

    i = sizeof(usb_device_descriptor) +
         (dev->descriptor.bNumConfigurations * sizeof(usb_config_descriptor));
    if (len < i) {
      printf( "device report too short - expected= %d  actual= %d  continuing...\n",
                     (int)i, (int)len);
      continue;
    }

    dev->config = (usb_config_descriptor *)calloc(
        dev->descriptor.bNumConfigurations, sizeof(usb_config_descriptor));
    if (!dev->config) {
      printf( "malloc\n");
      return (-1);
    }

    for (i = 0, len = sizeof(usb_device_descriptor);
         i < dev->descriptor.bNumConfigurations; i++) {

      char * ptr;

      ptr = &report[len];
      len += ((usb_config_descriptor *)ptr)->wTotalLength;
      rtn = usb_parse_configuration( &dev->config[i], ptr);

      if (rtn > 0)
        printf( "Descriptor data still left - bytes= %d\n", rtn);
      else
        if (rtn < 0)
          printf( "Unable to parse descriptors\n");
    } // configs

  } // devices

  *devices = devList;

  return (0);
}

/***************************************************************************/

int         usb_parse_configuration( usb_config_descriptor *config, char *buffer)

{
  int   i;
  int   retval;
  int   size;
  usb_descriptor_header *header;

  memcpy( config, buffer, USB_DT_CONFIG_SIZE);
  size = config->wTotalLength;

  if (config->bNumInterfaces > USB_MAXINTERFACES) {
    printf( "too many interfaces\n");
    return -1;
  }

  config->interface = (usb_interface *)
                       malloc( config->bNumInterfaces * sizeof(usb_interface));
  if (!config->interface) {
    printf( "malloc\n");
    return -1;      
  }

  memset( config->interface, 0, config->bNumInterfaces * sizeof(usb_interface));

  buffer += config->bLength;
  size -= config->bLength;
        
  // Skip over the rest of the Class or Vendor Specific descriptors
  for (i = 0; i < config->bNumInterfaces; i++) {
    char *begin;

    begin = buffer;
    while (size >= sizeof(usb_descriptor_header)) {
      header = (usb_descriptor_header *)buffer;

      if ((header->bLength > size) || (header->bLength < 2)) {
        printf( "invalid descriptor length of %d\n", header->bLength);
        return -1;
      }

      // If we find another "proper" descriptor then we're done
      if ((header->bDescriptorType == USB_DT_ENDPOINT) ||
          (header->bDescriptorType == USB_DT_INTERFACE) ||
          (header->bDescriptorType == USB_DT_CONFIG) ||
          (header->bDescriptorType == USB_DT_DEVICE))
        break;

      buffer += header->bLength;
      size -= header->bLength;
    }

    retval = usb_parse_interface(config->interface + i, buffer, size);
    if (retval < 0)
      return retval;

    buffer += retval;
    size -= retval;
  }

  return size;
}

/***************************************************************************/

int         usb_parse_interface( usb_interface *interface,
                                 unsigned char *buffer, int size)
{
  int   i;
  int   retval;
  int   parsed = 0;
  usb_descriptor_header *header;
  usb_interface_descriptor *ifp;
  unsigned char *begin;

  interface->num_altsetting = 0;

  while (size > 0) {
    interface->altsetting = realloc(interface->altsetting, sizeof(usb_interface_descriptor) * (interface->num_altsetting + 1));
    if (!interface->altsetting) {
      printf( "malloc\n");
      return -1;
    }

    ifp = interface->altsetting + interface->num_altsetting;
    interface->num_altsetting++;

    memcpy( ifp, buffer, USB_DT_INTERFACE_SIZE);

    /* Skip over the interface */
    buffer += ifp->bLength;
    parsed += ifp->bLength;
    size -= ifp->bLength;

    begin = buffer;

    /* Skip over any interface, class or vendor descriptors */
    while (size >= sizeof(usb_descriptor_header)) {
      header = (usb_descriptor_header *)buffer;

      if (header->bLength < 2) {
        printf( "invalid descriptor length of %d\n", header->bLength);
        return -1;
      }

      /* If we find another "proper" descriptor then we're done */
      if ((header->bDescriptorType == USB_DT_INTERFACE) ||
          (header->bDescriptorType == USB_DT_ENDPOINT) ||
          (header->bDescriptorType == USB_DT_CONFIG) ||
          (header->bDescriptorType == USB_DT_DEVICE))
        break;

      buffer += header->bLength;
      parsed += header->bLength;
      size -= header->bLength;
    }

    // Did we hit an unexpected descriptor?
    header = (usb_descriptor_header *)buffer;
    if ((size >= sizeof(usb_descriptor_header)) &&
        ((header->bDescriptorType == USB_DT_CONFIG) ||
        (header->bDescriptorType == USB_DT_DEVICE)))
      return parsed;

    if (ifp->bNumEndpoints > USB_MAXENDPOINTS) {
      printf( "too many endpoints\n");
      return -1;
    }

    ifp->endpoint = (usb_endpoint_descriptor *)
                     malloc( ifp->bNumEndpoints * sizeof(usb_endpoint_descriptor));
    if (!ifp->endpoint) {
      printf( "malloc\n");
      return -1;      
    }

    memset( ifp->endpoint, 0, ifp->bNumEndpoints * sizeof(usb_endpoint_descriptor));

    for (i = 0; i < ifp->bNumEndpoints; i++) {
      header = (usb_descriptor_header *)buffer;

      if (header->bLength > size) {
        printf( "ran out of descriptors parsing\n");
        return -1;
      }
                
      retval = usb_parse_endpoint(ifp->endpoint + i, buffer, size);
      if (retval < 0)
        return retval;

      buffer += retval;
      parsed += retval;
      size -= retval;
    }

    // We check to see if it's an alternate to this one
    ifp = (usb_interface_descriptor *)buffer;
    if (size < USB_DT_INTERFACE_SIZE ||
        ifp->bDescriptorType != USB_DT_INTERFACE ||
        !ifp->bAlternateSetting)
      return parsed;
  }

  return parsed;
}

/***************************************************************************/

int         usb_parse_endpoint( usb_endpoint_descriptor *endpoint,
                                unsigned char *buffer, int size)

{
  usb_descriptor_header *header;
  unsigned char *begin;
  int parsed = 0;

  header = (usb_descriptor_header *)buffer;

  // Everything should be fine being passed into here,
  // but we sanity check JIC
  if (header->bLength > size) {
    printf( "ran out of descriptors parsing\n");
    return -1;
  }
                
  if (header->bDescriptorType != USB_DT_ENDPOINT) {
    printf( "unexpected descriptor 0x%x, expecting endpoint descriptor, type 0x%x\n",
                   endpoint->bDescriptorType, USB_DT_ENDPOINT);
    return parsed;
  }
  if (header->bLength == USB_DT_ENDPOINT_AUDIO_SIZE)
    memcpy(endpoint, buffer, USB_DT_ENDPOINT_AUDIO_SIZE);
  else
    memcpy(endpoint, buffer, USB_DT_ENDPOINT_SIZE);
        
  buffer += header->bLength;
  size -= header->bLength;
  parsed += header->bLength;

  // Skip over the rest of the Class or Vendor Specific descriptors
  begin = buffer;

  while (size >= sizeof(usb_descriptor_header)) {
    header = (usb_descriptor_header *)buffer;

    if (header->bLength < 2) {
      printf( "invalid descriptor length of %d\n", header->bLength);
      return -1;
    }

    // If we find another "proper" descriptor then we're done
    if ((header->bDescriptorType == USB_DT_ENDPOINT) ||
        (header->bDescriptorType == USB_DT_INTERFACE) ||
        (header->bDescriptorType == USB_DT_CONFIG) ||
        (header->bDescriptorType == USB_DT_DEVICE))
      break;

    buffer += header->bLength;
    size -= header->bLength;
    parsed += header->bLength;
  }

  return parsed;
}

/***************************************************************************/
/***************************************************************************/

void        usb_free_dev( usb_device *dev)
{
  usb_destroy_configuration(dev);
  free(dev);
}

/***************************************************************************/

void        usb_destroy_configuration( usb_device *dev)
{
  int c, i, j;
        
  if (!dev->config)
    return;

  for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
     usb_config_descriptor *cf = &dev->config[c];

    if (!cf->interface)
      break;

    for (i = 0; i < cf->bNumInterfaces; i++) {
       usb_interface *ifp = &cf->interface[i];
                                
      if (!ifp->altsetting)
        break;

      for (j = 0; j < ifp->num_altsetting; j++) {
         usb_interface_descriptor *as = &ifp->altsetting[j];
                                        
        if (!as->endpoint)
          break;
                                        
        free(as->endpoint);
      }

      free(ifp->altsetting);
    }

    free(cf->interface);
  }

  free(dev->config);
}

/***************************************************************************/
/***************************************************************************/

