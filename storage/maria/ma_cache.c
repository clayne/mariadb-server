/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA */

/*
  Functions for read record cacheing with maria
  Used for reading dynamic/compressed records from datafile.

  Can fetch data directly from file (outside cache),
  if reading a small chunk straight before the cached part (with possible
  overlap).

  Can be explicitly asked not to use cache (by not setting READING_NEXT in
  flag) - useful for occasional out-of-cache reads, when the next read is
  expected to hit the cache again.

  Allows "partial read" errors in the record header (when READING_HEADER flag
  is set) - unread part is bzero'ed

  Note: out-of-cache reads are enabled for shared IO_CACHE's too,
  as these reads will be cached by OS cache (and my_pread is always atomic)
*/


#include "maria_def.h"

my_bool _ma_read_cache(MARIA_HA *handler, IO_CACHE *info, uchar *buff,
                       my_off_t pos, size_t length, uint flag)
{
  size_t read_length,in_buff_length;
  my_off_t offset;
  uchar *in_buff_pos;
  DBUG_ENTER("_ma_read_cache");
  DBUG_ASSERT(!(info->myflags & MY_ENCRYPT));

  if (unlikely(pos >= info->end_of_file) && (flag & READING_HEADER))
    DBUG_RETURN(-1);

  if (pos < info->pos_in_file)
  {
    read_length=length;
    if ((my_off_t) read_length > (my_off_t) (info->pos_in_file-pos))
      read_length=(uint) (info->pos_in_file-pos);
    info->seek_not_done=1;
    if (mysql_file_pread(info->file,buff,read_length,pos,MYF(MY_NABP)))
      DBUG_RETURN(1);
    if (!(length-=read_length))
      DBUG_RETURN(0);
    pos+=read_length;
    buff+=read_length;
  }
  if (pos >= info->pos_in_file &&
      (offset= (my_off_t) (pos - info->pos_in_file)) <
      (my_off_t) (info->read_end - info->request_pos))
  {
    in_buff_pos=info->request_pos+(uint) offset;
    in_buff_length= MY_MIN(length,(size_t) (info->read_end-in_buff_pos));
    memcpy(buff,info->request_pos+(uint) offset,(size_t) in_buff_length);
    if (!(length-=in_buff_length))
      DBUG_RETURN(0);
    pos+=in_buff_length;
    buff+=in_buff_length;
  }
  else
    in_buff_length=0;
  if (flag & READING_NEXT)
  {
    if (pos != (info->pos_in_file +
		(uint) (info->read_end - info->request_pos)))
    {
      info->pos_in_file=pos;				/* Force start here */
      info->read_pos=info->read_end=info->request_pos;	/* Everything used */
      info->seek_not_done=1;
    }
    else
      info->read_pos=info->read_end;			/* All block used */
    if (!_my_b_read(info,buff,length))
      DBUG_RETURN(0);
    read_length=info->error;
  }
  else
  {
    info->seek_not_done=1;
    if ((read_length=mysql_file_pread(info->file,buff,length,pos,MYF(0))) == length)
      DBUG_RETURN(0);
  }
  if (!(flag & READING_HEADER) || (int) read_length == -1 ||
      read_length+in_buff_length < 3)
  {
    if ((flag & READING_HEADER) && read_length + in_buff_length == 0)
      DBUG_RETURN(-1);                          /* End of file */

    DBUG_PRINT("error",
               ("Error %d reading next-multi-part block (Got %d of %d bytes)",
                my_errno, (int) read_length, (int) length));
    if (!my_errno || my_errno == HA_ERR_FILE_TOO_SHORT)
    {
      if (!handler->in_check_table)
        _ma_set_fatal_error(handler->s, HA_ERR_FILE_TOO_SHORT);
      if (!my_errno)
        my_errno= HA_ERR_WRONG_IN_RECORD;
    }
    DBUG_RETURN(1);
  }
  bzero(buff+read_length,MARIA_BLOCK_INFO_HEADER_LENGTH - in_buff_length -
        read_length);
  DBUG_RETURN(0);
} /* _ma_read_cache */
