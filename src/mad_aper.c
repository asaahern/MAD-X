#include "madx.h"

// static interface

#define MIN_DOUBLE 1.e-36

static struct aper_e_d* true_tab;

/* struct aper_e_d* offs_tab;*/
static struct table* offs_tab;


/* next function replaced 19 june 2007 BJ */
/* Improved for potential zero divide 20feb08 BJ */

static int
aper_rectellipse(double* ap1, double* ap2, double* ap3, double* ap4, int* quarterlength, double tablex[], double tabley[])
{
  double x, y, angle, alfa, theta, dangle;
  int i,napex;

  /*  printf("++ %10.5f  %10.5f  %10.5f  %10.5f\n",*ap1,*ap2,*ap3,*ap4);*/

  if ( *ap1 < MIN_DOUBLE*1.e10 || *ap2 < MIN_DOUBLE*1.e10) {
    fatal_error("Illegal zero or too small value value for ap1 or ap2"," ");
  }
  if ( *ap3 < MIN_DOUBLE*1.e10 || *ap4 < MIN_DOUBLE*1.e10) {
    fatal_error("Illegal zero or too small value value for ap3 or ap4"," ");
  }


  /* Produces a table of only the first quadrant coordinates */
  /* aper_fill_quads() completes the polygon          */

  if (*quarterlength) napex=9;
  else napex=19;

  /* PIECE OF USELESS CODE NOW THAT CASE RECTANGLE IS CORRECT, BJ 28feb08
     special case : rectangle
     does not work -- to be reworked

     if ( *ap3 < 0.) {
     tablex[0]=(*ap1);
     tabley[0]=(*ap2-0.0001);
     tablex[1]=(*ap1-0.00002);
     tabley[1]=(*ap2-0.00002);
     tablex[2]=(*ap1-0.0001);
     tabley[2]=(*ap2);
     *quarterlength=2;
     return 0;
     }
     END OF PIECE OF USELESS CODE, to be removed after some use
     (I expect no negative feedback ...)
  */

  /* find angles where rectangle and circle crosses */

  if ( (*ap1) >= (*ap3) ) {
    alfa = 0.;
  }
  else {
    y=sqrt((*ap3)*(*ap3)-(*ap1)*(*ap1));
    alfa=atan2(y,*ap1);
  }

  if ( (*ap2) >= (*ap4) ) {
    theta = 0.;
  }
  else {
    x=sqrt(((*ap3)*(*ap3)) * (1 - ((*ap2)*(*ap2)) / ((*ap4)*(*ap4))));
    y=sqrt((*ap3)*(*ap3)-x*x);
    theta=atan2(x,y);
  }

  dangle=(pi/2-(alfa+theta))/napex;

  if (!((0 < dangle) && (dangle < pi/2)))
  {
    return -1;
  }

  /*write coordinates for first quadrant*/

  for ( i=0 ; i<=napex; i++ ) {
    angle = alfa + i*dangle;
    tablex[i]=(*ap3)*cos(angle);
    tabley[i]=(*ap4)*sin(angle);

    if (i >= MAXARRAY/4)
    {
      fatal_error("Memory full in aper_rectellipse", "Number of coordinates exceeds set limit");      }
  }

  *quarterlength=i-1;

  return 0;
}

static void
aper_adj_quad(double angle, double x, double y, double* xquad, double* yquad)
{
  int quadrant;
  quadrant=angle/(pi/2)+1;
  switch (quadrant)
  {
    case 1: *xquad=x; *yquad=y; break;
    case 2: *xquad=-x; *yquad=y; break;
    case 3: *xquad=-x; *yquad=-y; break;
    case 4: *xquad=x; *yquad=-y; break;
  }
}

static void
aper_adj_halo_si(double ex, double ey, double betx, double bety, double bbeat, double halox[], double haloy[], int halolength, double haloxsi[], double haloysi[])
{
  int j;

  for (j=0;j<=halolength+1;j++)
  {
    haloxsi[j]=halox[j]*bbeat*sqrt(ex*betx);
    haloysi[j]=haloy[j]*bbeat*sqrt(ey*bety);
  }
}

static double
aper_online(double xm, double ym, double startx, double starty,
            double endx, double endy, double dist_limit)

/* BJ 13.02.2009.
   - added check |x-e| < dist_limit
   - removed useless calculations of sqrt
   - made consistent use of dist_limit and min_double */
/* BJ 13.02.2009
   check if point x = (xm,ym) is in the segment [s,e] with
   s = (startx,starty) and e = (endx,endy) by computing 
   cosfi = (x-s).(x-e) / |x-s||x-e|. cosfi = -1 : x is in
   first check if |x-s| and |x-e| are not too small.If yes for one of them : in
   if OK , the zero divide check must be superfluous. But keep it anyway.
*/

{
  double cosfi=1 , aaa, sss, eee;

  sss = sqrt((xm-startx)*(xm-startx)+ (ym-starty)*(ym-starty));
  eee = sqrt((xm-endx)*(xm-endx)    + (ym-endy)*(ym-endy));

  if ( sss <= dist_limit)
  {
    cosfi=-1;
  }
  else if ( eee <= dist_limit)
  {
    cosfi=-1;
  }
  else
  {
    aaa = sss * eee ;
    if ( aaa < MIN_DOUBLE )
      {
	fatal_error("Attempt to zero divide ", "In aper_online");
      }

    cosfi=  ((xm-startx)*(xm-endx)+(ym-starty)*(ym-endy)) / aaa;
  }

  if (cosfi <= -1+dist_limit)
  {
    cosfi=-1;
  }
  return cosfi;
}

/* NEW VERSION of aper_race, 20feb08 BJ, potential zero-divide issues cleared */

static void
aper_race(double xshift, double yshift, double r, double angle, double* x, double* y)
{
  double angle0, angle1, angle2, alfa, gamma, theta;
  int quadrant;

  /* this function calculates the displacement of the beam centre
     due to tolerance uncertainty for every angle */

  quadrant=angle/(pi/2)+1;

  if (xshift==0 && yshift==0 && r==0)
  {
    *x=0; *y=0;
    return;
  }

  switch (quadrant) /*adjusting angle to first quadrant*/
  {
    case 1: angle=angle; break;
    case 2: angle=pi-angle; break;
    case 3: angle=angle-pi; break;
    case 4: angle=twopi-angle; break;
  }

  if (angle==pi/2)
  {
    *x=0;
    *y=yshift+r;
  }
  else
  {
/*finding where arc starts and ends*/
    angle0=atan2( yshift , xshift+r );
    angle1=atan2( r+yshift , xshift );

    /*different methods is needed, depending on angle*/
    if (angle <= angle0 + MIN_DOUBLE * 1.e10 )
    {
      *x=xshift+r;
      *y=tan(angle)*(xshift+r);
    }
    else if (angle<angle1)
    {
/* if this is a circle, angle2 useless */
      if (!xshift && !yshift)  angle2 = 0;
      else angle2 = atan2( yshift , xshift );

      alfa = fabs(angle-angle2);
      if (alfa < MIN_DOUBLE * 1.e10)
      {
/* alfa==0 is a simpler case */
        *x=cos(angle)*(r+sqrt(xshift*xshift+yshift*yshift));
        *y=sin(angle)*(r+sqrt(xshift*xshift+yshift*yshift));
      }
      else
      {
/* solving sine rule w.r.t. gamma */
        gamma=asin(sqrt(xshift*xshift+yshift*yshift)/r*sin(alfa));
/*theta is the last corner in the triangle*/
        theta=pi-(alfa+gamma);
        *x=cos(angle)*r*sin(theta)/sin(alfa);
        *y=sin(angle)*r*sin(theta)/sin(alfa);
      }
    }
/* upper flat part */
    else
    {
      *y=r+yshift;
      *x=(r+yshift)*tan(pi/2-angle);
    }
  }
}

static int
aper_chk_inside(double p, double q, double pipex[], double pipey[], double dist_limit, int pipelength)
{
  int i;
  double n12, salfa, calfa, alfa=0;

  /*checks first whether p,q is exact on a pipe coordinate*/
  for (i=0;i<=pipelength;i++)
  {
    if (-1 == aper_online(p, q, pipex[i], pipey[i], pipex[i+1], pipey[i+1], dist_limit))
    {
      return 0;
    }
  }

  /*calculates and adds up angle from centre between all coordinates*/
  for (i=0;i<=pipelength;i++)
  {
    n12=sqrt(((pipex[i]-p)*(pipex[i]-p) + (pipey[i]-q)*(pipey[i]-q))
             * ((pipex[i+1]-p)*(pipex[i+1]-p) + (pipey[i+1]-q)*(pipey[i+1]-q)));

    salfa=((pipex[i]-p)*(pipey[i+1]-q) - (pipey[i]-q)*(pipex[i+1]-p))/n12;

    calfa=((pipex[i]-p)*(pipex[i+1]-p) + (pipey[i]-q)*(pipey[i+1]-q))/n12;

    alfa += atan2(salfa, calfa);
  }

  /*returns yes to main if total angle is at least twopi*/
  if (sqrt(alfa*alfa)>=(twopi-dist_limit))
  {
    return 1;
  }

  return 0;
}

static void
aper_intersect(double a1, double b1, double a2, double b2, double x1, double y1, double x2, double y2,
               int ver1, int ver2, double *xm, double *ym)
{
  (void)y1;
  
  if (ver1 && ver2 && x1==x2)
  {
    *xm=x2;
    *ym=y2;
  }
  else if (ver1)
  {
    *xm=x1;
    *ym=a2*x1+b2;
  }
  else if (ver2)
  {
    *xm=x2;
    *ym=a1*x2+b1;
  }
  else
  {
    *xm=(b1-b2)/(a2-a1);
    *ym=a1*(*xm)+b1;
  }
}

static int
aper_linepar(double x1,double y1,double x2,double y2,double *a,double *b)
{
  int vertical=0;

  if ( fabs(x1-x2)< MIN_DOUBLE)
  {
    *a=0;
    *b=y1-(*a)*x1;
    vertical=1;
  } else {
    *a=(y1-y2)/(x1-x2);
    *b=y1-(*a)*x1;
  }

  return vertical;
}

static void
aper_fill_quads(double polyx[], double polyy[], int quarterlength, int* halolength)
{
  int i=quarterlength+1, j;

  /* receives two tables with coordinates for the first quadrant */
  /* and mirrors them across x and y axes                         */

  /*copying first quadrant coordinates to second quadrant*/
  for (j=quarterlength;j>=0;j--)
  {
    polyx[i]=polyx[j];
    polyy[i]=polyy[j];
    aper_adj_quad(pi/2, polyx[i], polyy[i], &polyx[i], &polyy[i]);
    i++;
  }

  /*copying first quadrant coordinates to third quadrant*/
  for (j=0;j<=quarterlength;j++)
  {
    polyx[i]=polyx[j];
    polyy[i]=polyy[j];
    aper_adj_quad(pi, polyx[i], polyy[i], &polyx[i], &polyy[i]);
    i++;
  }

  /*copying first quadrant coordinates to fourth quadrant*/
  for (j=quarterlength;j>=0;j--)
  {
    polyx[i]=polyx[j];
    polyy[i]=polyy[j];
    aper_adj_quad(pi*3/2, polyx[i], polyy[i], &polyx[i], &polyy[i]);
    i++;
  }

  /*sets the last point equal to the first, to complete the shape.
    Necessary for compatibility with aper_calc function*/
  polyx[i]=polyx[0];
  polyy[i]=polyy[0];

  *halolength=i-1;
}

static void
aper_read_twiss(char* table, int* jslice, double* s, double* x, double* y,
                double* betx, double* bety, double* dx, double* dy)
{
  double_from_table(table, "s", jslice, s);
  double_from_table(table, "x", jslice, x);
  double_from_table(table, "y", jslice, y);
  double_from_table(table, "betx", jslice, betx);
  double_from_table(table, "bety", jslice, bety);
  double_from_table(table, "dx", jslice, dx);
  double_from_table(table, "dy", jslice, dy);
}

static int
aper_external_file(char *file, double tablex[], double tabley[])
{
  /* receives the name of file containing coordinates. Puts coordinates into tables. */
  int i=0;
  FILE *filept;

  if (file != NULL)
  {
    if ((filept=fopen(file, "r")) == NULL)
    {
      warning("Can not find file: ", file);
      return -1;
    }

    /*start making table*/
    while (2==fscanf(filept, "%lf %lf", &tablex[i], &tabley[i]))
    {
      i++;
      if (i >= MAXARRAY)
      {
        fatal_error("Memory full. ", "Number of coordinates exceeds set limit");
      }
    }

    tablex[i]=tablex[0];
    tabley[i]=tabley[0];
    fclose(filept);
  }
  return i-1;
}

static int
aper_bs(char* apertype, double* ap1, double* ap2, double* ap3, double* ap4, int* pipelength, double pipex[], double pipey[])
{
  int i, err, quarterlength=0;

  /* "var1 .. 4" represents values in the aperture array of each element  */
  /*  After they are read:                                                */
  /* *ap1 = half width rectangle                                          */
  /* *ap2 = half height rectangle                                         */
  /* *ap3 = half horizontal axis ellipse                                  */
  /* *ap4 = half vertical axis ellipse                                    */
  /*      returns 1 on success, 0 on failure          */

  (*ap1)=(*ap2)=(*ap3)=(*ap4)=0;

  /*   printf("-- #%s#\n",apertype); */

  if (!strcmp(apertype,"circle"))
  {
    *ap3=get_aperture(current_node, "var1"); /*radius circle*/

    *ap1 = *ap2 = *ap4 = *ap3;

    if (*ap3) /* check if r = 0, skip calc if r = 0 */
    {
      err=aper_rectellipse(ap1, ap2, ap3, ap4, &quarterlength, pipex, pipey);
      if (!err) aper_fill_quads(pipex, pipey, quarterlength, pipelength);
    }
    else err = -1;
  }

  else if (!strcmp(apertype,"ellipse"))
  {
    *ap3 = get_aperture(current_node, "var1"); /*half hor axis ellipse*/
    *ap4 = get_aperture(current_node, "var2"); /*half ver axis ellipse*/

    *ap1 = *ap3; *ap2 = *ap4;

    err=aper_rectellipse(ap1, ap2, ap3, ap4, &quarterlength, pipex, pipey);
    if (!err) aper_fill_quads(pipex, pipey, quarterlength, pipelength);
  }

  else if (!strcmp(apertype,"rectangle"))
  {

    *ap1 = get_aperture(current_node, "var1");      /*half width rect*/
    *ap2 = get_aperture(current_node, "var2");      /*half height rect*/
/* next changed 28 feb 2008 BJ */
    *ap3 = *ap4 = sqrt((*ap1) * (*ap1) + ((*ap2) * (*ap2))) - 1.e-15;
    /*   printf("-- %10.5f  %10.5f  %10.5f  %10.5f\n",*ap1,*ap2,*ap3,*ap4); */

    err=aper_rectellipse(ap1, ap2, ap3, ap4, &quarterlength, pipex, pipey);
    if (!err) aper_fill_quads(pipex, pipey, quarterlength, pipelength);
  }

  else if (!strcmp(apertype,"lhcscreen"))
  {
    *ap1=get_aperture(current_node, "var1"); /*half width rect*/
    *ap2=get_aperture(current_node, "var2"); /*half height rect*/
    *ap3=get_aperture(current_node, "var3"); /*radius circle*/

    (*ap4) = (*ap3);

    err=aper_rectellipse(ap1, ap2, ap3, ap4, &quarterlength, pipex, pipey);
    if (!err) aper_fill_quads(pipex, pipey, quarterlength, pipelength);
  }

  else if (!strcmp(apertype,"marguerite"))
  {
    printf("\nApertype %s not yet supported.", apertype);
    err=-1;
  }

  else if (!strcmp(apertype,"rectellipse"))
  {
    *ap1=get_aperture(current_node, "var1"); /*half width rect*/
    *ap2=get_aperture(current_node, "var2"); /*half height rect*/
    *ap3=get_aperture(current_node, "var3"); /*half hor axis ellipse*/
    *ap4=get_aperture(current_node, "var4"); /*half ver axis ellipse*/

    if (*ap1==0) /*this will not be 0 in the future*/
    {
      *ap1=*ap3;
    }
    if (*ap2==0) /*this will not be 0 in the future*/
    {
      *ap2=*ap4;
    }

    err=aper_rectellipse(ap1, ap2, ap3, ap4, &quarterlength, pipex, pipey);
    if (!err) aper_fill_quads(pipex, pipey, quarterlength, pipelength);
  }

  else if (!strcmp(apertype,"racetrack"))
  {
    *ap1=get_aperture(current_node, "var1"); /*half width rect*/
    *ap2=get_aperture(current_node, "var2"); /*half height rect*/
    *ap3=get_aperture(current_node, "var3"); /*radius circle*/

    *ap4 = *ap3;

    err=aper_rectellipse(ap3, ap3, ap3, ap4, &quarterlength, pipex, pipey);

    if (!err)
    {
      /*displaces the quartercircle*/
      for (i=0;i<=quarterlength;i++)
      {
        pipex[i] += (*ap1);
        pipey[i] += (*ap2);
      }

      aper_fill_quads(pipex, pipey, quarterlength, pipelength);
    }
  }

  else if (strlen(apertype))
  {
    *pipelength = aper_external_file(apertype, pipex, pipey);
    *ap1 = *ap2 = *ap3 = *ap4 = 0;
    if (*pipelength > -1) err=0; else err=-1;
  }

  else
  {
    *pipelength = -1;
    err=-1;
  }

  return err+1;
}

static int
aper_tab_search(int cnt, struct aper_e_d* tab, char* name, int* pos)
{
  /* looks for node *name in tab[], returns 1 if found, and its pos */
  int i=-1, found=0;

  while (i < cnt && found == 0)
  {
    i++;
    if (strcmp(name,tab[i].name) == 0) found=1;
  }
  *pos=i;

  return found;
}

static int
aper_tab_search_tfs(struct table* t, char* name, double* row)
{
  /* looks for node *name in tab[], returns 1 if found, and its pos
     NAME                        S_IP         X_OFF        DX_OFF       DDX_OFF         Y_OFF        DY_OFF       DDY_OFF
     function value return:
     1 ok
     0  not found
     -1 column does not exist
  */

  int i=0, found=0;
  int name_pos, s_ip_pos, x_off_pos, dx_off_pos, ddx_off_pos, y_off_pos, dy_off_pos, ddy_off_pos;

  /* get column positions */
  mycpy(c_dum->c, "name");
  if ((name_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "s_ip");
  if ((s_ip_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "x_off");
  if ((x_off_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "dx_off");
  if ((dx_off_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "ddx_off");
  if ((ddx_off_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "y_off");
  if ((y_off_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "dy_off");
  if ((dy_off_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;
  mycpy(c_dum->c, "ddy_off");
  if ((ddy_off_pos = name_list_pos(c_dum->c, t->columns)) < 0) return -1;

  /* printf("\n column names ok %d\n",t->curr);*/

  while (i < t->curr && found == 0)
    {
      i++;
      if( !strcmp(t->s_cols[name_pos][i-1],name)) {
      row[1] = t->d_cols[s_ip_pos][i-1];
      row[2] = t->d_cols[x_off_pos][i-1];
      row[3] = t->d_cols[dx_off_pos][i-1];
      row[4] = t->d_cols[ddx_off_pos][i-1];
      row[5] = t->d_cols[y_off_pos][i-1];
      row[6] = t->d_cols[dy_off_pos][i-1];
      row[7] = t->d_cols[ddy_off_pos][i-1];
      found = 1;
      }
    }

  return found;

}

static int
aper_e_d_read(char* e_d_name, struct aper_e_d** e_d_tabp, int* cnt, char* refnode)
{
  /* Reads data for special displacements of some magnets */
  int i=1, j, k, l, e_d_flag=0, curr_e_d_max = E_D_LIST_CHUNK, new_e_d_max;
  char comment[100]="empty";
  char *strpt;
  FILE *e_d_pt;
  struct aper_e_d* e_d_tab_loc;
  struct aper_e_d* e_d_tab = *e_d_tabp;


  if (e_d_name != NULL)
  {
    if((e_d_pt = fopen(e_d_name,"r")) == NULL)
    {
      printf("\nFile does not exist: %s\n",e_d_name);
    }
    else
    {
      /* part for reading reference node */
      while (strncmp(comment,"reference:",10) && i != EOF)
      {
        /*fgets(buf, 100, e_d_pt);*/
        i = fscanf(e_d_pt, "%s", comment);
        stolower(comment);
      }

      if (i == EOF) rewind(e_d_pt);
      else
      {
        if (strlen(comment) != 10)
        {
          strpt=strchr(comment,':');
          strpt++;
          strcpy(refnode, strpt);
        }
        else i = fscanf(e_d_pt, "%s", refnode);

        stolower(refnode);
        strcat(refnode, ":1");
      }
      /* printf("\nReference node: %s\n",refnode);*/
      /* end reading reference node */

      i=0;

      while (i != EOF)
      {
        i=fscanf(e_d_pt, "%s", e_d_tab[*cnt].name);
        /*next while-loop treats comments*/
        while ( e_d_tab[*cnt].name[0] == '!' && i != EOF)
        {
          fgets(comment, 100, e_d_pt);
          i=fscanf(e_d_pt, "%s", e_d_tab[*cnt].name);
        }

        stolower(e_d_tab[*cnt].name);

        if (i != EOF)
        {
          strcat(e_d_tab[*cnt].name, ":1");

          k=0; j=3;
          while (j == 3 && k < E_D_MAX)
          {
            j=fscanf(e_d_pt, "%lf %lf %lf", &e_d_tab[*cnt].tab[k][0],
                     &e_d_tab[*cnt].tab[k][1],
                     &e_d_tab[*cnt].tab[k][2]);
            k++;

            if (e_d_tab[*cnt].curr == E_D_MAX) printf("\nToo many points of x,y displacement...\n");
          }

          e_d_tab[*cnt].curr=k-2;

          (*cnt)++;

          if (*cnt == curr_e_d_max) /* grow e_d array */
          {
            /* printf("\nToo many special elements...(less than %d expected)\n", E_D_MAX); */
            new_e_d_max = curr_e_d_max + E_D_LIST_CHUNK;
            printf("\ngrowin e_d_max array to %d\n", new_e_d_max);

            e_d_tab_loc = (struct aper_e_d*) mycalloc("Aperture",new_e_d_max,sizeof(struct aper_e_d) );

            for( l=0 ; l < curr_e_d_max; l++)
            {
              e_d_tab_loc[l] = e_d_tab[l];
            }


            myfree("Aperture",e_d_tab);

            e_d_tab = e_d_tab_loc;

            curr_e_d_max = new_e_d_max;

          }

          i=j;
        }
      } /* while !EOF */

      printf("\nUsing extra displacements from file \"%s\"\n",e_d_name);
      e_d_flag=1; fclose(e_d_pt);
      (*cnt)--;
    }
  }

  *e_d_tabp = e_d_tab;

  return e_d_flag;
}

static struct table*
aper_e_d_read_tfs(char* e_d_name, int* cnt, char* refnode)
{
  /* Reads displacement data in tfs format */
  struct table* t = NULL;
  struct char_p_array* tcpa = NULL;
  struct name_list* tnl = NULL;
  int i, k, error = 0;
  short  sk;
  char *cc, *tmp, *name;
  int tempcount;
      tempcount = 0;

  (void)cnt;

  if (e_d_name == NULL) return NULL;

  printf("\n Reading offsets from tfs \"%s\"\n",e_d_name);


  if ((tab_file = fopen(e_d_name, "r")) == NULL)
    {
      warning("cannot open file:", e_d_name); return NULL;
    }

  while (fgets(aux_buff->c, aux_buff->max, tab_file))
    {
     tempcount++;

     cc = strtok(aux_buff->c, " \"\n");

     if (*cc == '@')
       {
       if ((tmp = strtok(NULL, " \"\n")) != NULL
           && strcmp(tmp, "REFERENCE") == 0) /* search for reference node */
        {
         if ((name = strtok(NULL, " \"\n")) != NULL) /* skip format */
           {
           if ((name = strtok(NULL, " \"\n")) != NULL) {
             strcpy(refnode, name);
             stolower(refnode);
             strcat(refnode, ":1");
           }
           }
        }
       /* printf("\n+++++ Reference node: %s\n",refnode); */
       }


     else if (*cc == '*' && tnl == NULL)
       {
	 tnl = new_name_list("table_names", 20);
	 while ((tmp = strtok(NULL, " \"\n")) != NULL)
	   add_to_name_list(permbuff(stolower(tmp)), 0, tnl);
       }
     else if (*cc == '$' && tcpa == NULL)
       {

      if (tnl == NULL)
        {
         warning("formats before names","skipped"); return NULL;
        }
      tcpa = new_char_p_array(20);
        while ((tmp = strtok(NULL, " \"\n")) != NULL)
        {
         if (tcpa->curr == tcpa->max) grow_char_p_array(tcpa);
           if (strcmp(tmp, "%s") == 0)       tnl->inform[tcpa->curr] = 3;
           else if (strcmp(tmp, "%hd") == 0) tnl->inform[tcpa->curr] = 1;
           else if (strcmp(tmp, "%d") == 0)  tnl->inform[tcpa->curr] = 1;
           else                              tnl->inform[tcpa->curr] = 2;
           tcpa->p[tcpa->curr++] = permbuff(tmp);
        }
       }
     else
       {
        if(t == NULL)
          {
         if (tcpa == NULL)
           {
            warning("TFS table without formats,","skipped"); error = 1;
           }
         else if (tnl == NULL)
           {
            warning("TFS table without column names,","skipped"); error = 1;
           }
         else if (tnl->curr == 0)
           {
            warning("TFS table: empty column name list,","skipped");
              error = 1;
           }
         else if (tnl->curr != tcpa->curr)
           {
            warning("TFS table: number of names and formats differ,",
                       "skipped");
              error = 1;
           }
           if (error)
           {
            delete_name_list(tnl); return NULL;
           }
           if(e_d_name != NULL) {
             t = new_table(e_d_name, "OFFSETS",    500, tnl);
           } else {
             t = new_table(e_d_name, "OFFSETS",    500, tnl);
           }
	   t->curr = 0;
        }
 
      for (i = 0; i < tnl->curr; i++)
        {
         if (t->curr == t->max) grow_table(t);
         tmp = tcpa->p[i];
           if (strcmp(tmp,"%s") == 0)  {
           t->s_cols[i][t->curr] = stolower(tmpbuff(cc));
           strcat(t->s_cols[i][t->curr], ":1");
         }
           else if (strcmp(tmp,"%d") == 0 )
           {
            sscanf(cc, tmp, &k); t->d_cols[i][t->curr] = k;
           }
           else if (strcmp(tmp,"%hd") == 0 )
           {
            sscanf(cc, tmp, &sk); t->d_cols[i][t->curr] = sk;
           }
           else sscanf(cc, tmp, &t->d_cols[i][t->curr]);
           if (i+1 < tnl->curr)
           {
              if ((cc =strtok(NULL, " \"\n")) == NULL)
              {
               warning("incomplete table line starting with:", aux_buff->c);
                 return NULL;
              }
           }
        }
        t->curr++;
       }
    }

  fclose(tab_file);
  t->origin = 1;
  /*  next line commented : avoid memory error at 2nd APERTURE command */
  /*  when the offset file has the same same, BJ 8apr2009 */
  /*  add_to_table_list(t, table_register);*/
  return t;
}

static void
aper_header(struct table* aper_t, struct aper_node *lim_)
  /* puts beam and aperture parameters at start of the aperture table */
{
  int i, err, nint=1, h_length = 25;
  double dtmp, vtmp[4], deltap_twiss, n1min, n1, s;
  char tmp[NAME_L], name[NAME_L], *stmp;

  n1 = lim_->n1;
  s  = lim_->s;
  strncpy(name, lim_->name, sizeof name);
  printf("\n\nWRITE HEADER : APERTURE LIMIT: %s, n1: %g, at: %g\n\n",name,n1,s);

  /* =================================================================*/
  /* ATTENTION: if you add header lines, augment h_length accordingly */
  /* =================================================================*/


  /* many modif to make the header being standard; BJ 25feb2008 */

  if (aper_t == NULL) return;
  stmp = command_par_string("pipefile", this_cmd->clone);
  if (stmp) h_length++;
  stmp = command_par_string("halofile", this_cmd->clone);
  if (stmp) h_length += 1; else h_length += 4;

  printf("\nheader %d \n",h_length);

  /* beam properties */
  if (aper_t->header == NULL)  aper_t->header = new_char_p_array(h_length);
  strncpy(tmp, current_sequ->name, sizeof tmp);
  sprintf(c_dum->c, v_format("@ SEQUENCE         %%%02ds \"%s\""),strlen(tmp),stoupper(tmp));
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  i = get_string("beam", "particle", tmp);
  sprintf(c_dum->c, v_format("@ PARTICLE         %%%02ds \"%s\""),i,stoupper(tmp));
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = get_value("beam", "mass");
  sprintf(c_dum->c, v_format("@ MASS             %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = get_value("beam", "energy");
  sprintf(c_dum->c, v_format("@ ENERGY           %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = get_value("beam", "pc");
  sprintf(c_dum->c, v_format("@ PC               %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = get_value("beam", "gamma");
  sprintf(c_dum->c, v_format("@ GAMMA            %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);


  /* aperture command properties */

  dtmp = command_par_value("exn", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ EXN              %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("eyn", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ EYN              %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("dqf", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ DQF              %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("betaqfx", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ BETAQFX          %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("dparx", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ PARAS_DX         %%le       %g"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("dpary", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ PARAS_DY         %%le       %g"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("dp", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ DP_BUCKET_SIZE   %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  err = double_from_table("summ","deltap",&nint,&deltap_twiss);
  sprintf(c_dum->c, v_format("@ TWISS_DELTAP     %%le  %F"), deltap_twiss);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);

  dtmp = command_par_value("cor", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ CO_RADIUS        %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("bbeat", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ BETA_BEATING     %%le  %F"), dtmp);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  dtmp = command_par_value("nco", this_cmd->clone);
  sprintf(c_dum->c, v_format("@ NB_OF_ANGLES     %%d   %g"), dtmp*4);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);

  /* if a filename with halo coordinates is given, need not show halo */
  stmp = command_par_string("halofile", this_cmd->clone);
  if (stmp)
  {
    strncpy(tmp, stmp, sizeof tmp);
    sprintf(c_dum->c, v_format("@ HALOFILE         %%%02ds \"%s\""),strlen(tmp),stoupper(tmp));
    aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  }
  else
  {
    i = command_par_vector("halo", this_cmd->clone, vtmp);
    sprintf(c_dum->c, v_format("@ HALO_PRIM        %%le       %g"),vtmp[0]);
    aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
    sprintf(c_dum->c, v_format("@ HALO_R           %%le       %g"),vtmp[1]);
    aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
    sprintf(c_dum->c, v_format("@ HALO_H           %%le       %g"),vtmp[2]);
    aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
    sprintf(c_dum->c, v_format("@ HALO_V           %%le       %g"),vtmp[3]);
    aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  }
  /* show filename with pipe coordinates if given */
  stmp = command_par_string("pipefile", this_cmd->clone);
  if (stmp)
  {
    strncpy(tmp, stmp, sizeof tmp);
    sprintf(c_dum->c, v_format("@ PIPEFILE         %%%02ds \"%s\""),strlen(tmp),stoupper(tmp));
    aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  }

  printf("\n\nWRITE HEADER : APERTURE LIMIT: %s, n1: %g, at: %g\n\n",name,n1,s);
  printf("\ncurr %d \n",aper_t->header->curr);


  sprintf(c_dum->c, v_format("@ n1min            %%le   %g"), n1);
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
  n1min = n1;
  set_value("beam","n1min",&n1min);

  sprintf(c_dum->c, v_format("@ at_element       %%%02ds  \"%s\""),strlen(name),stoupper(name) );
  aper_t->header->p[aper_t->header->curr++] = tmpbuff(c_dum->c);
}

static void
aper_surv(double init[], int nint)
{
  struct in_cmd* aper_survey;
  struct name_list* asnl;
  int aspos;

  /* Constructs artificial survey command, the result is the  */
  /* table 'survey' which can be accessed from all functions. */
  /* init[0] = x0, init[1] = y0, init[2] = z0,                */
  /* init[3] = theta0, init[4] = phi0, init[5] = psi0         */

  aper_survey = new_in_cmd(10);
  aper_survey->type = 0;
  aper_survey->clone = aper_survey->cmd_def =
    clone_command(find_command("survey",defined_commands));
  asnl = aper_survey->cmd_def->par_names;
  aspos = name_list_pos("table", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->string = "survey";
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

  aspos = name_list_pos("x0", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->double_value = init[0];
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

  aspos = name_list_pos("y0", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->double_value = init[1];
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

  aspos = name_list_pos("z0", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->double_value = init[2];
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

  aspos = name_list_pos("theta0", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->double_value = init[3];
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

  aspos = name_list_pos("phi0", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->double_value = init[4];
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

  aspos = name_list_pos("psi0", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->double_value = init[5];
  aper_survey->cmd_def->par_names->inform[aspos] = 1;

/* frs: suppressing the survey file created by the internal survey command */
  aspos = name_list_pos("file", asnl);
  aper_survey->cmd_def->par->parameters[aspos]->string = NULL;
  aper_survey->cmd_def->par_names->inform[aspos] = 0;

  current_survey=(aper_survey->clone);
  pro_survey(aper_survey);

  double_from_table("survey","x",&nint, &init[0]);
  double_from_table("survey","y",&nint, &init[1]);
  double_from_table("survey","z",&nint, &init[2]);
  double_from_table("survey","theta",&nint, &init[3]);
  double_from_table("survey","phi",&nint, &init[4]);
  double_from_table("survey","psi",&nint, &init[5]);
}

static void
aper_trim_ws(char* string, int len)
{
  int c=0;

  /* Replaces the first ws or : in a string with a '\0', */
  /* thus translating a FORTRAN-like attribute string to */
  /* C compatibility, or washes the ':1' from node names */

  while (string[c]!=' ' && string[c]!='\0' && c<=len) c++;

  string[c]='\0';
  if (c<len) string[c+1]=' '; /*adds a ws to avoid two \0 in a row*/
}



static void
aper_write_table(char* name, double* n1, double* n1x_m, double* n1y_m,
                  double* rtol, double* xtol, double* ytol,
                  char* apertype,double* ap1,double* ap2,double* ap3,double* ap4,
                  double* on_ap, double* on_elem, double* spec,double* s,
                  double* x, double* y, double* betx, double* bety,double* dx, double* dy,
                  char *table)
{
  string_to_table(table, "name", name);
  double_to_table(table, "n1", n1);
  double_to_table(table, "n1x_m", n1x_m);
  double_to_table(table, "n1y_m", n1y_m);
  double_to_table(table, "rtol", rtol);
  double_to_table(table, "xtol", xtol);
  double_to_table(table, "ytol", ytol);
  string_to_table(table, "apertype", apertype);
  double_to_table(table, "aper_1", ap1);
  double_to_table(table, "aper_2", ap2);
  double_to_table(table, "aper_3", ap3);
  double_to_table(table, "aper_4", ap4);
  double_to_table(table, "on_ap", on_ap);
  double_to_table(table, "on_elem", on_elem);
  double_to_table(table, "spec", spec);
  double_to_table(table, "s", s);
  double_to_table(table, "x", x);
  double_to_table(table, "y", y);
  double_to_table(table, "betx", betx);
  double_to_table(table, "bety", bety);
  double_to_table(table, "dx", dx);
  double_to_table(table, "dy", dy);

  augment_count(table);
}


static double
aper_calc(double p, double q, double* minhl, double halox[], double haloy[],
          int halolength,double haloxadj[],double haloyadj[],
          double newhalox[], double newhaloy[], double pipex[], double pipey[],
          int pipelength, double notsimple)
{
  int i=0, j=0, c=0, ver1, ver2;
  double dist_limit=0.0000000001;
  double a1, b1, a2, b2, xm, ym, h, l;

  for (c=0;c<=halolength+1;c++)
  {
    haloxadj[c]=halox[c]+p;
    haloyadj[c]=haloy[c]+q;
  }

  c=0;

  /*if halo centre is inside beam pipe, calculate smallest H/L ratio*/
  if (aper_chk_inside(p, q, pipex, pipey, dist_limit, pipelength))
  {
    if (notsimple)
    {
      /*Adds extra apexes first:*/
      for (j=0;j<=halolength;j++)
      {
        newhalox[c]=haloxadj[j];
        newhaloy[c]=haloyadj[j];
        c++;

        for (i=0;i<=pipelength;i++)
        {
          /*Find a and b parameters for line*/
          ver1=aper_linepar(p, q, pipex[i], pipey[i], &a1, &b1);
          ver2=aper_linepar(haloxadj[j], haloyadj[j],
                            haloxadj[j+1], haloyadj[j+1], &a2, &b2);

          /*find meeting coordinates for infinitely long lines*/
          aper_intersect(a1, b1, a2, b2, pipex[i], pipey[i],
                         haloxadj[j], haloyadj[j], ver1, ver2, &xm, &ym);

          /*eliminate intersection points not between line limits*/
          if (-1 == aper_online(xm, ym, haloxadj[j], haloyadj[j],
                                haloxadj[j+1], haloyadj[j+1], dist_limit)) /*halo line*/
          {
            if (-1 != aper_online(p, q, pipex[i], pipey[i], xm, ym,
                                  dist_limit))  /*test line*/
            {
              newhalox[c]=xm;
              newhaloy[c]=ym;
              c++;
            }
          }
        }
      }

      halolength=c-1;
      for (j=0;j<=halolength;j++)
      {
        haloxadj[j]=newhalox[j];
        haloyadj[j]=newhaloy[j];
      }

    }

    /*Calculates smallest ratio:*/
    for (i=0;i<=pipelength;i++)
    {
      for (j=0;j<=halolength;j++)
      {
        /*Find a and b parameters for line*/
        ver1=aper_linepar(p, q, haloxadj[j], haloyadj[j], &a1, &b1);
        ver2=aper_linepar(pipex[i], pipey[i], pipex[i+1], pipey[i+1], &a2, &b2);

        /*find meeting coordinates for infinitely long lines*/
        aper_intersect(a1, b1, a2, b2, haloxadj[j], haloyadj[j],
                       pipex[i], pipey[i], ver1, ver2, &xm, &ym);

        /*eliminate intersection points not between line limits*/
        if (-1 == aper_online(xm, ym, pipex[i], pipey[i], pipex[i+1], pipey[i+1],
                              dist_limit)) /*pipe line*/
        {
          if (-1 != aper_online(p, q, haloxadj[j], haloyadj[j], xm, ym,
                                dist_limit))  /*test line*/
          {
            h=sqrt((xm-p)*(xm-p)+(ym-q)*(ym-q));
            l=sqrt((haloxadj[j]-p)*(haloxadj[j]-p)
                   + (haloyadj[j]-q)*(haloyadj[j]-q));
            if (h/l < *minhl)
            {
              *minhl=h/l;
            }
          }
        }
      }
    }
  }
  else /*if halo centre is outside of beam pipe*/
  {
    *minhl=0;
    return -1;
  }

  return 0;
}

// public interface

double
get_apertol(struct node* node, char* par)
  /* returns aper_tol parameter 'i' where i is integer at the end of par;
     e.g. aptol_1 gives i = 1 etc. (count starts at 1) */
{
  int i, k, n = strlen(par);
  double val = zero, vec[100];
  for (i = 0; i < n; i++)  if(isdigit(par[i])) break;
  if (i == n) return val;
  sscanf(&par[i], "%d", &k); k--;
  if ((n = element_vector(node->p_elem, "aper_tol", vec)) > k)  val = vec[k];
  return val;
}

double 
get_aperture(struct node* node, char* par)
  /* returns aperture parameter 'i' where i is integer at the end of par;
     e.g. aper_1 gives i = 1 etc. (count starts at 1) */
{
  int i, k, n = strlen(par);
  double val = zero, vec[100];
  for (i = 0; i < n; i++)  if(isdigit(par[i])) break;
  if (i == n) return val;
  sscanf(&par[i], "%d", &k); k--;
  if ((n = element_vector(node->p_elem, "aperture", vec)) > k)  val = vec[k];
  return val;
}

void
pro_aperture(struct in_cmd* cmd)
{
  struct aper_node* limit_node;
  struct node *use_range[2];
  struct table* tw_cp;
  char *file, *range, tw_name[NAME_L], *table="aperture";
  int tw_cnt, rows;
  double interval;
  setbuf(stdout,(char *)NULL);

  embedded_twiss_cmd = cmd;

  /* check for valid sequence, beam and Twiss table */
  if (current_sequ != NULL && sequence_length(current_sequ) != zero)
  {
    if (attach_beam(current_sequ) == 0)
    {
      fatal_error("Aperture module - sequence without beam:",
                  current_sequ->name);
    }
  }
  else fatal_error("Aperture module - no active sequence:", current_sequ->name);

  if (current_sequ->tw_table == NULL)
  {
    warning("No TWISS table present","Aperture command ignored");
    return;
  }

  range = command_par_string("range", this_cmd->clone);
  if (get_ex_range(range, current_sequ, use_range) == 0)
  {
    warning("Illegal range.","Aperture command ignored");
    return;
  }
  current_node = use_range[0];

  /* navigate to starting point in Twiss table */
  tw_cp=current_sequ->tw_table;

  tw_cnt=1; /* table starts at 1 seen from char_from_table function */
  if (char_from_table(tw_cp->name, "name", &tw_cnt, tw_name) != 0)
  {
    warning("Erroneus Twiss table.","Aperture command ignored.");
    return;
  }
  aper_trim_ws(tw_name, NAME_L);
  while (strcmp(tw_name,current_node->name))
  {
    tw_cnt++;
    if (tw_cnt > tw_cp->curr)
    {
      warning("Could not find range start in Twiss table", "Aperture command ignored.");
      return;
    }
    char_from_table(tw_cp->name, "name", &tw_cnt, tw_name);
    aper_trim_ws(tw_name, NAME_L);
  }
  tw_cnt--; /* jumps back to "real" value */

  /* approximate # of needed rows in aperture table */
  interval = command_par_value("interval", this_cmd->clone);
  rows = current_sequ->n_nodes + 2 * (sequence_length(current_sequ)/interval);

  /* make empty aperture table */
  aperture_table=make_table(table, table, ap_table_cols, ap_table_types, rows);
  aperture_table->dynamic=1;
  add_to_table_list(aperture_table, table_register);

  /* calculate apertures and fill table */
  limit_node = aperture(table, use_range, tw_cp, &tw_cnt);

  if (limit_node->n1 != -1)
  {
    printf("\n\nAPERTURE LIMIT: %s, n1: %g, at: %g\n\n",
           limit_node->name,limit_node->n1,limit_node->s);
    aper_header(aperture_table, limit_node);
    
    file = command_par_string("file", this_cmd->clone);
    if (file != NULL)
    {
      out_table(table, aperture_table, file);
    }
    if (strcmp(aptwfile,"dummy")) out_table(tw_cp->name, tw_cp, aptwfile);
  }
  else warning("Could not run aperture command.","Aperture command ignored");

  /* set pointer to updated Twiss table */
  current_sequ->tw_table=tw_cp;
}

struct aper_node*
aperture(char *table, struct node* use_range[], struct table* tw_cp, int *tw_cnt)
{
  int stop=0, nint=1, jslice=1, err, first, ap=1;
  int true_flag, true_node=0, offs_node=0, do_survey=0;
  int truepos=0, true_cnt=0, offs_cnt=0;
  int halo_q_length=1, halolength, pipelength, namelen=NAME_L, nhalopar, ntol;
  double surv_init[6]={0, 0, 0, 0, 0, 0};
  double surv_x=zero, surv_y=zero, elem_x=0, elem_y=0;
  double xa=0, xb=0, xc=0, ya=0, yb=0, yc=0;
  double on_ap=1, on_elem=0;
  double mass, energy, exn, eyn, dqf, betaqfx, dp, dparx, dpary;
  double cor, bbeat, nco, halo[4], interval, spec, ex, ey, notsimple;
  double s=0, x=0, y=0, betx=0, bety=0, dx=0, dy=0, ratio, n1, nr, length;
  double xeff=0,yeff=0;
  double n1x_m, n1y_m;
  double s_start, s_curr, s_end;
  double node_s=-1, node_n1=-1;
  double aper_tol[3], ap1, ap2, ap3, ap4;
  double dispx, dispy, tolx, toly;
  double dispxadj=0, dispyadj=0, coxadj, coyadj, tolxadj=0, tolyadj=0;
  double angle, dangle, deltax, deltay;
  double xshift, yshift, r;
  double halox[MAXARRAY], haloy[MAXARRAY], haloxsi[MAXARRAY], haloysi[MAXARRAY];
  double haloxadj[MAXARRAY], haloyadj[MAXARRAY], newhalox[MAXARRAY], newhaloy[MAXARRAY];
  double pipex[MAXARRAY], pipey[MAXARRAY];
  double parxd,paryd,deltap_twiss;
  char *halofile, *truefile, *offsfile;
  char refnode[NAME_L];
  char *cmd_refnode;
  char apertype[NAME_L];
  char name[NAME_L];
  char tol_err_mess[80] = "";

  struct node* rng_glob[2];
  struct aper_node limit_node = {"none", -1, -1, "none", {-1,-1,-1,-1},{-1,-1,-1}};
  struct aper_node* lim_pt = &limit_node;

  int is_zero_len;

  true_tab = (struct aper_e_d*) mycalloc("Aperture",E_D_LIST_CHUNK,sizeof(struct aper_e_d) );
  /* offs_tab = (struct aper_e_d*) mycalloc("Aperture",E_D_LIST_CHUNK,sizeof(struct aper_e_d));*/

  setbuf(stdout,(char*)NULL);

  printf("\nProcessing apertures from %s to %s...\n",use_range[0]->name,use_range[1]->name);

  /* read command parameters */
  halofile = command_par_string("halofile", this_cmd->clone);
  /* removed IW 240205 */
  /*  pipefile = command_par_string("pipefile", this_cmd->clone); */
  exn = command_par_value("exn", this_cmd->clone);
  eyn = command_par_value("eyn", this_cmd->clone);
  dqf = command_par_value("dqf", this_cmd->clone);
  betaqfx = command_par_value("betaqfx", this_cmd->clone);
  dp = command_par_value("dp", this_cmd->clone);
  dparx = command_par_value("dparx", this_cmd->clone);
  dpary = command_par_value("dpary", this_cmd->clone);
  cor = command_par_value("cor", this_cmd->clone);
  bbeat = command_par_value("bbeat", this_cmd->clone);
  nco = command_par_value("nco", this_cmd->clone);
  nhalopar = command_par_vector("halo", this_cmd->clone, halo);
  interval = command_par_value("interval", this_cmd->clone);
  spec = command_par_value("spec", this_cmd->clone);
  notsimple = command_par_value("notsimple", this_cmd->clone);
  truefile = command_par_string("trueprofile", this_cmd->clone);
  offsfile = command_par_string("offsetelem", this_cmd->clone);

  cmd_refnode = command_par_string("refnode", this_cmd->clone);

  mass = get_value("beam", "mass");
  energy = get_value("beam", "energy");

  /* fetch deltap as set by user in the former TWISS command */
  /* will be used below for displacement associated to parasitic dipersion */

  err = double_from_table("summ","deltap",&nint,&deltap_twiss);
  printf ("+++++++ deltap from TWISS %12.6g\n",deltap_twiss);

  /* calculate emittance and delta angle */
  ex=mass*exn/energy; ey=mass*eyn/energy;
  dangle=twopi/(nco*4);

  /* check if trueprofile and offsetelem files exist */
  true_flag = aper_e_d_read(truefile, &true_tab, &true_cnt, refnode);
  /* offs_flag = aper_e_d_read(offsfile, &offs_tab, &offs_cnt, refnode);*/
  offs_tab = aper_e_d_read_tfs(offsfile, &offs_cnt, refnode);


  if (cmd_refnode != NULL) 
    {
      strcpy(refnode, cmd_refnode);
      strcat(refnode, ":1");
    }

  printf("\nreference node: %s\n",refnode);

  /* build halo polygon based on input ratio values or coordinates */
  if ((halolength = aper_external_file(halofile, halox, haloy)) > -1) ;
  else if (aper_rectellipse(&halo[2], &halo[3], &halo[1], &halo[1], &halo_q_length, halox, haloy))
  {
    warning("Not valid parameters for halo. ", "Unable to make polygon.");

    /* IA */
    myfree("Aperture",true_tab);
    myfree("Aperture",offs_tab);

    return lim_pt;
  }
  else aper_fill_quads(halox, haloy, halo_q_length, &halolength);

  /* check for externally given pipe polygon */
  /* changed this recently, IW 240205 */
  /*  pipelength = aper_external_file(pipefile, pipex, pipey);
      if ( pipelength > -1) ext_pipe=1; */

  /* get initial twiss parameters, from start of first element in range */
  aper_read_twiss(tw_cp->name, tw_cnt, &s_end, &x, &y, &betx, &bety, &dx, &dy);
  (*tw_cnt)++;
  aper_adj_halo_si(ex, ey, betx, bety, bbeat, halox, haloy, halolength, haloxsi, haloysi);

  /* calculate initial normal+parasitic disp. */
  /* modified 27feb08 BJ */
  parxd = dparx*sqrt(betx/betaqfx)*dqf;
  paryd = dpary*sqrt(bety/betaqfx)*dqf;

  /* Initialize n1 limit value */
  lim_pt->n1=999999;

  while (!stop)
  {
    strcpy(name,current_node->name);
    aper_trim_ws(name, NAME_L);

    is_zero_len = 0;

    /* the first node in a sequence can not be sliced, hence: */
    if (current_sequ->range_start == current_node) first=1; else first=0;

    length=node_value("l");
    err=double_from_table(current_sequ->tw_table->name, "s", tw_cnt, &s_end);
    s_start=s_end-length;
    s_curr=s_start;

    node_string("apertype", apertype, &namelen);
    aper_trim_ws(apertype, NAME_L);


    if (!strncmp("drift",name,5))
    {
      on_elem=-999999;
    }
    else on_elem=1;

    if ( (offs_tab!= NULL) && (strcmp(refnode, name) == 0)) do_survey=1;
    /* printf("\nname: %s, ref: %s, do_survey:: %d\n",name,refnode, do_survey);*/


    /* read data for tol displacement of halo */
    get_node_vector("aper_tol",&ntol,aper_tol);
    if (ntol == 3)
    {
      r = aper_tol[0];
      xshift = aper_tol[1];
      yshift = aper_tol[2];
    }
    else r=xshift=yshift=0;

    /*read aperture data and make polygon tables for beam pipe*/
    /* IW 250205 */
    /*  if (ext_pipe == 0) */
    ap=aper_bs(apertype, &ap1, &ap2, &ap3, &ap4, &pipelength, pipex, pipey);

    if (ap == 0 || first == 1)
    {
      /* if no pipe can be built, the n1 is set to inf and Twiss parms read for reference*/
      n1=999999; n1x_m=999999; n1y_m=999999; on_ap=-999999; nint=1;

      aper_read_twiss(tw_cp->name, tw_cnt, &s_end,
                      &x, &y, &betx, &bety, &dx, &dy);

      aper_write_table(name, &n1, &n1x_m, &n1y_m, &r, &xshift, &yshift, apertype,
                       &ap1, &ap2, &ap3, &ap4, &on_ap, &on_elem, &spec,
                       &s_end, &x, &y, &betx, &bety, &dx, &dy, table);
      on_ap=1;

      double_to_table_row(tw_cp->name, "n1", tw_cnt, &n1);
      (*tw_cnt)++;

      /* calc disp and adj halo to have ready for next node */
      /* modified 27feb08 BJ */
      parxd = dparx*sqrt(betx/betaqfx)*dqf;
      paryd = dpary*sqrt(bety/betaqfx)*dqf;

      aper_adj_halo_si(ex, ey, betx, bety, bbeat, halox, haloy, halolength, haloxsi, haloysi);

      /*do survey to have ready init for next node */
      if (do_survey)
      {
        rng_glob[0] = current_sequ->range_start;
        rng_glob[1] = current_sequ->range_end;
        current_sequ->range_start = current_sequ->range_end = current_node;
        aper_surv(surv_init, nint);
        double_from_table("survey","x",&nint, &surv_x);
        double_from_table("survey","y",&nint, &surv_y);
        current_sequ->range_start = rng_glob[0];
        current_sequ->range_end = rng_glob[1];
      }
    }    /* end loop 'if no pipe ' */

    else
    {
      node_n1=999999;
      true_node=0;
      offs_node=0;

      /* calculate the number of slices per node */
      if (true_flag == 0)
      {
        nint=length/interval;
      }
      else
      {
        true_node=aper_tab_search(true_cnt, true_tab, name, &truepos);

        if (true_node)
        {
          nint=true_tab[truepos].curr;
        }
        else nint=length/interval;
      }
      /* printf("\nname: %s, nint: %d",name,nint); */

      if (!nint) nint=1;

      /* don't interpolate 0-length elements*/


      if (fabs(length) < MIN_DOUBLE ) is_zero_len = 1;


      /* slice the node, call survey if necessary, make twiss for slices*/
      err=interp_node(&nint);

      /* do survey */
      if (do_survey)
      {
        double offs_row[8] = { 0 };

        /* printf("\n using offsets\n");*/

        aper_surv(surv_init, nint);

        offs_node=aper_tab_search_tfs(offs_tab, name, offs_row);
        if (offs_node)
        {
          /* printf("\nusing offset");*/
          xa=offs_row[4];
          xb=offs_row[3];
          xc=offs_row[2];
          ya=offs_row[7];
          yb=offs_row[6];
          yc=offs_row[5];
      }
        /* else {
        printf("\nsearch returned: %d",offs_node);
        } */


      }

      err=embedded_twiss();

      /* Treat each slice, for all angles */
      for (jslice=0;jslice<=nint;jslice++)
      {
        ratio=999999;
        if (jslice != 0)
        {
          aper_read_twiss("embedded_twiss_table", &jslice, &s, &x, &y,
                          &betx, &bety, &dx, &dy);

          s_curr=s_start+s;
          aper_adj_halo_si(ex, ey, betx, bety, bbeat, halox, haloy, halolength,
                           haloxsi, haloysi);

          /* calculate normal+parasitic disp.*/
          /* modified 27feb08 BJ */
          parxd = dparx*sqrt(betx/betaqfx)*dqf;
          paryd = dpary*sqrt(bety/betaqfx)*dqf;

          if (do_survey)
          {
            double_from_table("survey","x",&jslice, &surv_x);
            double_from_table("survey","y",&jslice, &surv_y);
          }
        }
        else
        /*  jslice==0, parameters from previous node will be used  */
        {
          s_curr+=1.e-12;     /*to get correct plot at start of elements*/
          s=0;                /*used to calc elem_x elem_y) */
        }

	/*       printf ("%20s  s %10.6f / x %10.6f   y %10.6f\n",name,s,x,y); */

	xeff = x;
	yeff = y;
	  
        /* survey adjustments */
       /* BJ 3 APR 2009 : introduced xeff and yeff in order to avoid
          interferences with x and y as used for the first slice 
	  (re-use from end of former node) */
        if (offs_node)
        {
          elem_x=xa*s*s+xb*s+xc;
          elem_y=ya*s*s+yb*s+yc;
          xeff=x+(surv_x-elem_x);
          yeff=y+(surv_y-elem_y);
        }

        /* discrete adjustments */
        if (true_node)
        {
          xeff+=true_tab[truepos].tab[jslice][1];
          yeff+=true_tab[truepos].tab[jslice][2];
        }

        for (angle=0;angle<twopi;angle+=dangle)
        {
          /* new 27feb08 BJ */
          dispx = bbeat*(fabs(dx)*dp + parxd*(fabs(deltap_twiss)+dp) );
          dispy = bbeat*(fabs(dy)*dp + paryd*(fabs(deltap_twiss)+dp) );

          /*adjust dispersion to worst-case for quadrant*/
          aper_adj_quad(angle, dispx, dispy, &dispxadj, &dispyadj);

          /*calculate displacement co+tol for each angle*/
          coxadj=cor*cos(angle); coyadj=cor*sin(angle);

          /* Error check added 20feb08 BJ */
          if ( xshift<0 || yshift < 0 || r<0 ) {
            sprintf(tol_err_mess,"In element : %s\n",name);
            fatal_error("Illegal negative tolerance",tol_err_mess);
          }
          aper_race(xshift,yshift,r,angle,&tolx,&toly);

          aper_adj_quad(angle, tolx, toly, &tolxadj, &tolyadj);

          /* add all displacements */
          deltax = coxadj + tolxadj + xeff + dispxadj;
          deltay = coyadj + tolyadj + yeff + dispyadj;

          /* send beta adjusted halo and its displacement to aperture calculation */
          aper_calc(deltax,deltay,&ratio,haloxsi,haloysi,
                    halolength,haloxadj,haloyadj,newhalox,newhaloy,
                    pipex,pipey,pipelength,notsimple);
        }

        nr=ratio*halo[1];
        n1=nr/(halo[1]/halo[0]); /* ratio r/n = 1.4 */

        n1x_m=n1*bbeat*sqrt(betx*ex);
        n1y_m=n1*bbeat*sqrt(bety*ey);

/* Change below, BJ 23oct2008                              */
/* test block 'if (n1 < node_n1)' included in test block   */
/*   if ( (is_zero_len == 0) ...'                          */

        if ( (is_zero_len == 0) || (jslice == 1) ) {

          aper_write_table(name, &n1, &n1x_m, &n1y_m, &r, &xshift, &yshift, apertype,
                           &ap1, &ap2, &ap3, &ap4, &on_ap, &on_elem, &spec, &s_curr,
                           &xeff, &yeff, &betx, &bety, &dx, &dy, table);

        /* save node minimum n1 */

        if (n1 < node_n1)
          {
            node_n1=n1; node_s=s_curr;
          }

	} /* end if 'is_zero_len ... */

      }

      err=reset_interpolation(&nint);

      /* insert minimum node value into Twiss table */

      double_to_table_row(tw_cp->name, "n1", tw_cnt, &node_n1);
      (*tw_cnt)++;

      /* save range minimum n1 */
      if (node_n1 < lim_pt->n1)
      {
        strcpy(lim_pt->name,name);
        lim_pt->n1=node_n1;
        lim_pt->s=node_s;
        strcpy(lim_pt->apertype,apertype);
        lim_pt->aperture[0]=ap1;
        lim_pt->aperture[1]=ap2;
        lim_pt->aperture[2]=ap3;
        lim_pt->aperture[3]=ap4;
        lim_pt->aper_tol[0]=r;
        lim_pt->aper_tol[1]=xshift;
        lim_pt->aper_tol[2]=yshift;
      }
    }

    if (!strcmp(current_node->name,use_range[1]->name)) stop=1;
    if (!advance_node()) stop=1;
  }

  myfree("Aperture",true_tab);
  if (offs_tab != NULL) myfree("Aperture",offs_tab);

  return lim_pt;
}
