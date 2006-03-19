module madx_ptc_tablepush_module
!This module enables the user to store any variable of PTC in a MAD-X table
!what gives access to them from the other modules, which the most important is MATCH
  use madx_keywords
  implicit none
  save
  public

!============================================================================================  
!  PUBLIC INTERFACE
  public                                      :: putusertable
  public                                      :: addpush


!============================================================================================
!  PRIVATE 
!    data structures
  type tablepush_poly
     character(20) :: tabname  !table name  to put the coefficients
     character(20) :: colname  !column name to put the coefficients
     integer       :: element  !defines the element of 6D function
     character(10) :: monomial !defiens the monomial f.g. 100000 is coeff of x, and 020300 is coeff of px^2*py^3
  end type tablepush_poly
 
!    routines
  type (tablepush_poly), private, dimension(20) :: pushes
  integer                                       :: npushes = 0
  character(20), private, dimension(20)         :: tables  !tables names of existing pushes - each is listed only ones
  integer,       private                        :: ntables = 0 !number of distictive tables 

contains
!____________________________________________________________________________________________

subroutine putusertable(n,y)
!puts the coefficients in tables as defined in array pushes
  implicit none
  integer         :: n !fibre number
  type(real_8),target  :: y(6)!input 6 dimensional function (polynomial)
  type(real_8),pointer :: e !element in array
  real(dp)        :: coeff
  integer         :: i,ii !iterator
  integer         :: at !iterator
  
!    print *,"madx_ptc_tablepush :putusertable "
!    call daprint(y(1),6)

    do i=1,npushes

      e => y(pushes(i)%element)
      coeff = e.sub.(pushes(i)%monomial)
!      write(6,'(a13, a10, a3, f9.6, a10, i1, 5(a13), i3)') &
!       &        "Putting coef ",pushes(i)%monomial,"=",coeff," arr_row ", pushes(i)%element,&
!       &        " in table ", pushes(i)%tabname," at column ", pushes(i)%colname, &
!       &        " for fibre no ",n

      call double_to_table(pushes(i)%tabname, pushes(i)%colname, coeff);
       
    enddo
   
    call augment_counts() 

end subroutine putusertable
!____________________________________________________________________________________________

subroutine augment_counts()
 implicit none
 integer  :: i ! iterator

   do i=1,ntables
!     print *,"Augmenting ",tables(i)
     call augmentcountonly(tables(i)) !we need to use special augement, 
                                      !cause the regular one looks for for
                                      !variables names like columns to fill the table
   enddo
 
end subroutine augment_counts
!____________________________________________________________________________________________

subroutine addpush(table,column,element,monomial)
  implicit none
  include 'twissa.fi'
  integer   table(*)
  integer   column(*)
  integer   element
  integer   monomial(*)
    
    npushes = npushes + 1
    pushes(npushes)%tabname = charconv(table)
    pushes(npushes)%colname = charconv(column)
    pushes(npushes)%element = element
    pushes(npushes)%monomial = charconv(monomial)
    
    pushes(npushes)%tabname(table(1)+1:table(1)+1)=achar(0)
    pushes(npushes)%colname(column(1)+1:column(1)+1)=achar(0)
    pushes(npushes)%monomial(monomial(1)+1:monomial(1)+1)=achar(0)
    
!    print  *,"madx_ptc_tablepush : addpush(",&
!   &          pushes(npushes)%tabname,">,<",pushes(npushes)%colname,">,<",&
!   &          pushes(npushes)%element,">,<",pushes(npushes)%monomial,">)"
  
   if ( issuchtableexist(pushes(npushes)%tabname) .eqv. .false.) then
     ntables = ntables + 1
     tables(ntables) = pushes(npushes)%tabname
!     print *,"Table has been added to the tables list ", tables(ntables)
   endif
     
end subroutine addpush
!____________________________________________________________________________________________


logical function issuchtableexist(tname)
 implicit none
 character(20) :: tname !name of the table to be checked if already is listed in table names array
 integer       :: i! iterator 

 issuchtableexist = .false.

 do i=1, ntables
   if (pushes(i)%tabname == tname) then
     issuchtableexist = .true.
     return
   endif
 enddo

end function issuchtableexist 
!____________________________________________________________________________________________
 

end module madx_ptc_tablepush_module
