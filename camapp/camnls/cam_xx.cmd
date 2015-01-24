/***************************************************************************/
/* cam_xx.cmd - (C) Copyright 2006,2007 R.L.Walsh                          */
/* Released under the terms of the Mozilla Public License Version 1.1      */
/* You may obtain a copy of the License at http://www.mozilla.org/MPL/     */
/***************************************************************************/

ARG lang .

if LENGTH(lang) <> 2 then do
    say ''
    say '  cam_xx.cmd creates a language dll for Cameraderie v1.5.'
    say ''
    say '  1 - translate the English words and phrases in CAM_XX.TXT.'
    say '  2 - run CAM_XX.CMD from a command line, using the 2-letter'
    say '      code for your language as a parameter - for example:'
    say '          "cam_xx NL" for Dutch'
    say '  3 - if successful, cam_xx.cmd will create a new dll for'
    say '      your language (e.g. cam_NL.dll) and will copy it to'
    say '      the main Cameraderie directory'
    say '  4 - reopen Cameraderie and see how it looks'
    say ''
    say '  Note:  you can run this script as many times as you want -'
    say '         a new version of the dll will created each time.'
    exit
end

newbase = 'cam_'||lang
newname = newbase||'.dll'
error = 1

do 1

    say ''
    say '*** Creating' newname '***'

    /* make a copy of the empty dll & rename it */
    '@copy camnls.dll' newname '> NUL 2>&1'
    if RC <> 0 then do
        say '*** There was an error copying camnls.dll to' newname '***'
        say 'Ensure camnls.dll is in the current directory, then try again.'
        leave
    end

    /* compile the resources */
    '@rc -n -r camnls.rc'
    if RC <> 0 then do
        say '*** There was an error creating camnls.res ***'
        say 'Please review the error message(s), edit cam_xx.txt, then try again.'
        leave
    end

    /* bind the resources to the new dll */
    '@rc -n -x2 camnls.res' newname
    if RC <> 0 then do
        say '*** There was an error attaching camnls.res to the dll ***'
        leave
    end

    /* delete the .res from the current directory */
    '@del camnls.res > NUL 2>&1'

    /* change the dll's module name to match the filename */
    rc = RenameModule(newname)
    if RC <> 0 then do
        say '*** There was an error patching' newname '- rc =' rc '***'
        leave
    end

    error = 0
    say ''
    say '***' newname 'was successfuly created ***'

    /* if the app can't be found in the current directory's parent, */
    /* the user will have to copy the dll to the correct location */
    rc = stream('..\camera.exe','c','query exists')
    if rc = '' then do
        say 'Please copy it to the main Cameraderie directory'
        leave
    end

    /* copy the dll to the parent directory */
    '@copy' newname '..\* > NUL 2>&1'
    if RC <> 0 then do
        say 'Please copy it to the main Cameraderie directory'
        leave
    end

    /* delete the dll from the current directory */
    '@del' newname '> NUL 2>&1'

    say 'It has been moved to the main Cameraderie'
    say 'directory and is ready to be used'
end

/* in case of error, delete the defective dll */
if error <> 0 then
    '@del' newname '> NUL 2>&1'

exit

/***************************************************************************/
/* RenameModule() changes a dll's module name to match its file name.      */
/*    Input:  the dll's qualified or unqualified filename;  the new        */
/*            module name may not be longer than the name it replaces.     */
/*    Output: returns zero if successful or an errorcode if it fails.      */
/***************************************************************************/

RenameModule:  procedure

arg dllname
rtn = 0
offs = 0
opened = 0
NUMERIC DIGITS 10

/* this dummy 'do' statement lets us jump to the end using 'leave' */
do 1

    /* confirm the dll exists */
    rc = stream(dllname,'c','query exists')
    if rc = '' then do
        rtn = 1
        leave
    end

    /* open the dll & set the open flag if successful*/
    rc = stream(dllname,'c','open')
    if rc \= 'READY:' then do
        rtn = 2
        leave
    end
    open = 1

    /* if there's an MZ header, use it to locate the LX header */
    rc = charin(dllname,1,2)
    if rc = 'MZ' then do
        rc = charin(dllname,25,2)
        rc = reverse(rc)
        rc = c2d(rc)
        if rc \= 64 then do
            rtn = 3
            leave
        end

        rc = charin(dllname,61,4)
        rc = reverse(rc)
        rc = c2d(rc)
        offs = rc + 1
    end

    /* 'offs' should now point at the LX header - if not exit */
    rc = charin(dllname,offs,2)
    if rc \= 'LX' then do
        rtn = 4
        leave
    end

    /* get the offset of the resident names table */
    /* its first entry is the module name */
  	rc = charin(dllname,offs+88,4)
    rc = reverse(rc)
    rc = c2d(rc)
    offs = offs + rc

    /* get the length of the existing module name */
    /* the new name cannot be any longer than this */
  	rc = charin(dllname,offs,1)
  	maxlth = c2d(rc)

    /* extract the new module name from the file name */
    rc = filespec('n',dllname)
    parse upper value rc with modname '.' .

    /* ensure the new name is not longer than the existing name */
    modlth = length(modname)
    if modlth > maxlth then do
        rtn = 5
        leave
    end

    /* if the new name is shorter than the original, pad it with nulls */
    /* then insert the new name's actual length before the mod name */
    if modlth < maxlth then
        modname = left(modname,maxlth,d2c(0))
    modname = insert(d2c(modlth,1),modname)

    /* overwrite the existing length & name with the new values */
    rc = charout(dllname,modname, offs)
    if rc \= 0 then do
        rtn = 6
        leave
    end

end

/* if the stream was opened, close it */
if open = 1 then
    rc = stream(dllname,'c','close')

return rtn

/***************************************************************************/

