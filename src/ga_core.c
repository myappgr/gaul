/**********************************************************************
  ga_core.c
 **********************************************************************

  ga_core - Genetic algorithm routines.
  Copyright ©2000-2003, Stewart Adcock <stewart@linux-domain.com>
  All rights reserved.

  The latest version of this program should be available at:
  http://www.stewart-adcock.co.uk/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.  Alternatively, if your project
  is incompatible with the GPL, I will probably agree to requests
  for permission to use the terms of any other license.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY WHATSOEVER.

  A full copy of the GNU General Public License should be in the file
  "COPYING" provided with this distribution; if not, see:
  http://www.gnu.org/

 **********************************************************************

  Synopsis:     Routines for handling populations and performing GA
		operations.

		Also contains a number of helper functions providing
		alternative optimisation schemes for comparison and
		analysis purposes.

		BEWARE: NOT THREAD-SAFE!

		Internally, and in the public C interface, pointers
		are used to identify the population and entity
		structures.  However, in the scripting interface these
		pointers are unusable, so identifing integers are
		used instead.

  Vague usage details:	Set-up with ga_genesis().
			Perform calculations with ga_evolution().
			Grab data for post-analysis with ga_transcend().
			Evolution will continue if ga_evolution() is
			called again without calling ga_genesis() again.

  To do:	Reading/writing 'soup' files.
		Replace the send_mask int array with a bit vector.
		All functions here should be based on entity/population _pointers_ while the functions in ga_intrinsics should be based on _handles_.
		More "if ( !pop ) die("Null pointer to population structure passed.");" checks are needed.
		Population/entity iterator functions.
		ga_get_struct_whatever() should be renamed to ga_struct_get_whatever().

 **********************************************************************/

#include "ga_core.h"

/*
 * Global variables.
 */
THREAD_LOCK_DEFINE_STATIC(pop_table_lock);
static TableStruct	*pop_table=NULL;	/* The population table. */

/*
 * Lookup table for functions.
 *
 * This is required for saving defined code hooks in files and for
 * some script interfaces.
 */
struct func_lookup {char *funcname; void *func_ptr;};

static struct func_lookup lookup[]={
	{ NULL, NULL },
	{ "ga_select_one_random", (void *) ga_select_one_random },
	{ "ga_select_two_random", (void *) ga_select_two_random },
	{ "ga_select_one_every", (void *) ga_select_one_every },
	{ "ga_select_two_every", (void *) ga_select_two_every },
	{ "ga_select_one_randomrank", (void *) ga_select_one_randomrank },
	{ "ga_select_two_randomrank", (void *) ga_select_two_randomrank },
	{ "ga_select_one_bestof2", (void *) ga_select_one_bestof2 },
	{ "ga_select_two_bestof2", (void *) ga_select_two_bestof2 },
	{ "ga_select_one_roulette", (void *) ga_select_one_roulette },
	{ "ga_select_two_roulette", (void *) ga_select_two_roulette },
	{ "ga_select_one_roulette_rebased", (void *) ga_select_one_roulette_rebased },
	{ "ga_select_two_roulette_rebased", (void *) ga_select_two_roulette_rebased },
	{ "ga_select_one_sus", (void *) ga_select_one_sus },
	{ "ga_select_two_sus", (void *) ga_select_two_sus },
	{ "ga_crossover_integer_singlepoints", (void *) ga_crossover_integer_singlepoints },
	{ "ga_crossover_integer_doublepoints", (void *) ga_crossover_integer_doublepoints },
	{ "ga_crossover_integer_mixing", (void *) ga_crossover_integer_mixing },
	{ "ga_crossover_integer_allele_mixing", (void *) ga_crossover_integer_allele_mixing },
	{ "ga_crossover_boolean_singlepoints", (void *) ga_crossover_boolean_singlepoints },
	{ "ga_crossover_boolean_doublepoints", (void *) ga_crossover_boolean_doublepoints },
	{ "ga_crossover_boolean_mixing", (void *) ga_crossover_boolean_mixing },
	{ "ga_crossover_boolean_allele_mixing", (void *) ga_crossover_boolean_allele_mixing },
	{ "ga_crossover_char_mixing", (void *) ga_crossover_char_mixing },
	{ "ga_crossover_char_allele_mixing", (void *) ga_crossover_char_allele_mixing },
	{ "ga_crossover_double_mixing", (void *) ga_crossover_double_mixing },
	{ "ga_crossover_double_allele_mixing", (void *) ga_crossover_double_allele_mixing },
	{ "ga_crossover_char_singlepoints", (void *) ga_crossover_char_singlepoints },
	{ "ga_crossover_char_doublepoints", (void *) ga_crossover_char_doublepoints },
	{ "ga_crossover_bitstring_singlepoints", (void *) ga_crossover_bitstring_singlepoints },
	{ "ga_crossover_bitstring_doublepoints", (void *) ga_crossover_bitstring_doublepoints },
	{ "ga_crossover_bitstring_mixing", (void *) ga_crossover_bitstring_mixing },
	{ "ga_crossover_bitstring_allele_mixing", (void *) ga_crossover_bitstring_allele_mixing },
	{ "ga_mutate_integer_singlepoint_drift", (void *) ga_mutate_integer_singlepoint_drift },
	{ "ga_mutate_integer_singlepoint_randomize", (void *) ga_mutate_integer_singlepoint_randomize },
	{ "ga_mutate_integer_multipoint", (void *) ga_mutate_integer_multipoint },
	{ "ga_mutate_integer_allpoint", (void *) ga_mutate_integer_allpoint },
	{ "ga_mutate_boolean_singlepoint", (void *) ga_mutate_boolean_singlepoint },
	{ "ga_mutate_boolean_multipoint", (void *) ga_mutate_boolean_multipoint },
	{ "ga_mutate_char_singlepoint_drift", (void *) ga_mutate_char_singlepoint_drift },
	{ "ga_mutate_char_singlepoint_randomize", (void *) ga_mutate_char_singlepoint_randomize },
	{ "ga_mutate_char_multipoint", (void *) ga_mutate_char_multipoint },
	{ "ga_mutate_printable_singlepoint_drift", (void *) ga_mutate_printable_singlepoint_drift },
	{ "ga_mutate_printable_singlepoint_randomize", (void *) ga_mutate_printable_singlepoint_randomize },
	{ "ga_mutate_printable_multipoint", (void *) ga_mutate_printable_multipoint },
	{ "ga_mutate_bitstring_singlepoint", (void *) ga_mutate_bitstring_singlepoint },
	{ "ga_mutate_double_singlepoint_randomize", (void *) ga_mutate_double_singlepoint_randomize },
	{ "ga_mutate_double_singlepoint_drift", (void *) ga_mutate_double_singlepoint_drift },
	{ "ga_seed_boolean_random", (void *) ga_seed_boolean_random },
	{ "ga_seed_integer_random", (void *) ga_seed_integer_random },
	{ "ga_seed_integer_zero", (void *) ga_seed_integer_zero },
	{ "ga_seed_double_random", (void *) ga_seed_double_random },
	{ "ga_seed_double_zero", (void *) ga_seed_double_zero },
	{ "ga_seed_char_random", (void *) ga_seed_char_random },
	{ "ga_seed_printable_random", (void *) ga_seed_printable_random },
	{ "ga_seed_bitstring_random", (void *) ga_seed_bitstring_random },
	{ "ga_replace_by_fitness", (void *) ga_replace_by_fitness },
	{ "ga_chromosome_integer_allocate", (void *) ga_chromosome_integer_allocate },
	{ "ga_chromosome_integer_deallocate", (void *) ga_chromosome_integer_deallocate },
	{ "ga_chromosome_integer_replicate", (void *) ga_chromosome_integer_replicate },
	{ "ga_chromosome_integer_to_bytes", (void *) ga_chromosome_integer_to_bytes },
	{ "ga_chromosome_integer_from_bytes", (void *) ga_chromosome_integer_from_bytes },
	{ "ga_chromosome_integer_to_string", (void *) ga_chromosome_integer_to_string },
	{ "ga_chromosome_boolean_allocate", (void *) ga_chromosome_boolean_allocate },
	{ "ga_chromosome_boolean_deallocate", (void *) ga_chromosome_boolean_deallocate },
	{ "ga_chromosome_boolean_replicate", (void *) ga_chromosome_boolean_replicate },
	{ "ga_chromosome_boolean_to_bytes", (void *) ga_chromosome_boolean_to_bytes },
	{ "ga_chromosome_boolean_from_bytes", (void *) ga_chromosome_boolean_from_bytes },
	{ "ga_chromosome_boolean_to_string", (void *) ga_chromosome_boolean_to_string },
	{ "ga_chromosome_double_allocate", (void *) ga_chromosome_double_allocate },
	{ "ga_chromosome_double_deallocate", (void *) ga_chromosome_double_deallocate },
	{ "ga_chromosome_double_replicate", (void *) ga_chromosome_double_replicate },
	{ "ga_chromosome_double_to_bytes", (void *) ga_chromosome_double_to_bytes },
	{ "ga_chromosome_double_from_bytes", (void *) ga_chromosome_double_from_bytes },
	{ "ga_chromosome_double_to_string", (void *) ga_chromosome_double_to_string },
	{ "ga_chromosome_char_allocate", (void *) ga_chromosome_char_allocate },
	{ "ga_chromosome_char_deallocate", (void *) ga_chromosome_char_deallocate },
	{ "ga_chromosome_char_replicate", (void *) ga_chromosome_char_replicate },
	{ "ga_chromosome_char_to_bytes", (void *) ga_chromosome_char_to_bytes },
	{ "ga_chromosome_char_from_bytes", (void *) ga_chromosome_char_from_bytes },
	{ "ga_chromosome_char_to_string", (void *) ga_chromosome_char_to_string },
	{ "ga_chromosome_bitstring_allocate", (void *) ga_chromosome_bitstring_allocate },
	{ "ga_chromosome_bitstring_deallocate", (void *) ga_chromosome_bitstring_deallocate },
	{ "ga_chromosome_bitstring_replicate", (void *) ga_chromosome_bitstring_replicate },
	{ "ga_chromosome_bitstring_to_bytes", (void *) ga_chromosome_bitstring_to_bytes },
	{ "ga_chromosome_bitstring_from_bytes", (void *) ga_chromosome_bitstring_from_bytes },
	{ "ga_chromosome_bitstring_to_string", (void *) ga_chromosome_bitstring_to_string },
	{ NULL, NULL } };


/**********************************************************************
  Private utility functions.
 **********************************************************************/

/**********************************************************************
  destruct_list()
  synopsis:	Destroys an userdata list and it's contents.  For
		many applications, the destructor callback will just
		be free() or similar.  This callback may safely be
		NULL.
  parameters:	population *pop	Population.
		SLList *list	Phenomic data.
  return:	none
  last updated:	24 Aug 2002
 **********************************************************************/

static void destruct_list(population *pop, SLList *list)
  {
  SLList        *this;		/* Current list element */
  int		num_destroyed;	/* Count number of things destroyed. */
  vpointer	data;		/* Data in item. */

/* A bit of validation. */
  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !list ) die("Null pointer to list passed.");

/*
 * Deallocate data stored in the list, if required.
 */
  if ( pop->data_destructor )
    {
    num_destroyed = 0;
    this=list;

    while(this!=NULL)
      {
      if ((data = slink_data(this)))
        {
        pop->data_destructor(data);
        num_destroyed++;
        }
      this=slink_next(this);
      }

#if GA_DEBUG>2
/*
 * Not typically needed now, because num_destrtoyed may
 * (correctly) differ from the actual number of chromosomes.
 */
    if (num_destroyed != pop->num_chromosomes)
      printf("Uh oh! Dodgy user data here? %d %d\n",
                 num_destroyed, pop->num_chromosomes);
#endif

    }

/* Deallocate the list sructure. */
  slink_free_all(list);

  return;
  }


/**********************************************************************
  Population handling functions.
 **********************************************************************/

/**********************************************************************
  ga_population_new()
  synopsis:	Allocates and initialises a new population structure,
		and assigns a new population id to it.
  parameters:	const int stable_size		Num. individuals carried into next generation.
		const int num_chromosome	Num. of chromosomes.
		const int len_chromosome	Size of chromosomes (may be ignored).
  return:	population *	new population structure.
  last updated: 07 Nov 2002
 **********************************************************************/

population *ga_population_new(	const int stable_size,
				const int num_chromosome,
				const int len_chromosome)
  {
  population	*newpop=NULL;	/* New population structure. */
  unsigned int	pop_id;		/* Handle for new population structure. */
  int		i;		/* Loop over (unassigned) entities. */

  if ( !(newpop = s_malloc(sizeof(population))) )
    die("Unable to allocate memory");

  newpop->size = 0;
  newpop->stable_size = stable_size;
  newpop->max_size = stable_size*4;
  newpop->orig_size = 0;
  newpop->num_chromosomes = num_chromosome;
  newpop->len_chromosomes = len_chromosome;
  newpop->data = NULL;
  newpop->free_index = newpop->max_size-1;
  newpop->island = -1;
  newpop->generation = 0;

  newpop->crossover_ratio = 1.0;
  newpop->mutation_ratio = 1.0;
  newpop->migration_ratio = 1.0;
  newpop->scheme = GA_SCHEME_DARWIN;
  newpop->elitism = GA_ELITISM_UNKNOWN;

  if ( !(newpop->entity_array = s_malloc(newpop->max_size*sizeof(entity*))) )
    die("Unable to allocate memory");

  if ( !(newpop->entity_iarray = s_malloc(newpop->max_size*sizeof(entity*))) )
    die("Unable to allocate memory");

  newpop->entity_chunk = mem_chunk_new(sizeof(entity), 512);

/*
 * Wipe the entity arrays.
 */
  for (i=0; i<newpop->max_size; i++)
    {
    newpop->entity_array[i] = NULL;
    newpop->entity_iarray[i] = NULL;
    }

/*
 * Wipe optional parameter data.
 */
  newpop->tabu_params = NULL;
  newpop->sa_params = NULL;
  newpop->climbing_params = NULL;
  newpop->simplex_params = NULL;
  newpop->gradient_params = NULL;
  newpop->search_params = NULL;
  
/*
 * Clean the callback functions.
 * Prevents erronerous callbacks - helpful when debugging!
 */
  newpop->generation_hook = NULL;
  newpop->iteration_hook = NULL;

  newpop->data_destructor = NULL;
  newpop->data_ref_incrementor = NULL;

  newpop->chromosome_constructor = NULL;
  newpop->chromosome_destructor = NULL;
  newpop->chromosome_replicate = NULL;
  newpop->chromosome_to_bytes = NULL;
  newpop->chromosome_from_bytes = NULL;
  newpop->chromosome_to_string = NULL;

  newpop->evaluate = NULL;
  newpop->seed = NULL;
  newpop->adapt = NULL;
  newpop->select_one = NULL;
  newpop->select_two = NULL;
  newpop->mutate = NULL;
  newpop->crossover = NULL;
  newpop->replace = NULL;

/*
 * Add this new population into the population table.
 */
  THREAD_LOCK(pop_table_lock);
  if ( !pop_table ) pop_table=table_new();

  pop_id = table_add(pop_table, (vpointer) newpop);
  THREAD_UNLOCK(pop_table_lock);

  plog( LOG_DEBUG, "New pop = %p id = %d", newpop, pop_id);

  return newpop;
  }


/**********************************************************************
  ga_population_clone_empty()
  synopsis:	Allocates and initialises a new population structure,
		and fills it with an exact copy of the data from an
		existing population, with the exception that entity
		data is not copied.  The population's user data
		field is referenced.
  parameters:	population *	original population structure.
  return:	population *	new population structure.
  last updated: 24 May 2002
 **********************************************************************/

population *ga_population_clone_empty(population *pop)
  {
  int		i;		/* Loop variable. */
  population	*newpop=NULL;	/* New population structure. */
  unsigned int	pop_id;		/* Handle for new population structure. */

  /* Checks */
  if ( !pop ) die("Null pointer to population structure passed.");

/*
 * Allocate new structure.
 */
  newpop = s_malloc(sizeof(population));

/*
 * Clone parameters.
 */
  newpop->size = 0;
  newpop->stable_size = pop->stable_size;
  newpop->max_size = pop->max_size;
  newpop->orig_size = 0;
  newpop->num_chromosomes = pop->num_chromosomes;
  newpop->len_chromosomes = pop->len_chromosomes;
  newpop->data = pop->data;
  newpop->free_index = pop->max_size-1;

  newpop->crossover_ratio = pop->crossover_ratio;
  newpop->mutation_ratio = pop->mutation_ratio;
  newpop->migration_ratio = pop->migration_ratio;

/*
 * Clone the callback functions.
 */
  newpop->generation_hook = pop->generation_hook;
  newpop->iteration_hook = pop->iteration_hook;

  newpop->data_destructor = pop->data_destructor;
  newpop->data_ref_incrementor = pop->data_ref_incrementor;

  newpop->chromosome_constructor = pop->chromosome_constructor;
  newpop->chromosome_destructor = pop->chromosome_destructor;
  newpop->chromosome_replicate = pop->chromosome_replicate;
  newpop->chromosome_to_bytes = pop->chromosome_to_bytes;
  newpop->chromosome_from_bytes = pop->chromosome_from_bytes;
  newpop->chromosome_to_string = pop->chromosome_to_string;

  newpop->evaluate = pop->evaluate;
  newpop->seed = pop->seed;
  newpop->adapt = pop->adapt;
  newpop->select_one = pop->select_one;
  newpop->select_two = pop->select_two;
  newpop->mutate = pop->mutate;
  newpop->crossover = pop->crossover;
  newpop->replace = pop->replace;

/*
 * Allocate arrays etc.
 */
  newpop->entity_array = s_malloc(newpop->max_size*sizeof(entity*));
  newpop->entity_iarray = s_malloc(newpop->max_size*sizeof(entity*));
  newpop->entity_chunk = mem_chunk_new(sizeof(entity), 512);
  
/*
 * Wipe the the entity arrays.
 */
  for (i=0; i<newpop->max_size; i++)
    {
    newpop->entity_array[i] = NULL;
    newpop->entity_iarray[i] = NULL;
    }

/*
 * Wipe optional parameter data.
 */
  newpop->tabu_params = NULL;
  newpop->sa_params = NULL;

/*
 * Add this new population into the population table.
 */
  THREAD_LOCK(pop_table_lock);
  if ( !pop_table ) pop_table=table_new();

  pop_id = table_add(pop_table, (vpointer) newpop);
  THREAD_UNLOCK(pop_table_lock);

  plog( LOG_DEBUG, "New pop = %p id = %d (cloned from %p)",
        newpop, pop_id, pop );

  return newpop;
  }


/**********************************************************************
  ga_population_clone()
  synopsis:	Allocates and initialises a new population structure,
		and fills it with an exact copy of the data from an
		existing population, including the individual
		entity data.  The population's user data
		field is referenced.
		Entity id's between the populations will _NOT_
		correspond.
  parameters:	population *	original population structure.
  return:	population *	new population structure.
  last updated: 24 May 2002
 **********************************************************************/

population *ga_population_clone(population *pop)
  {
  int		i;		/* Loop variable. */
  population	*newpop=NULL;	/* New population structure. */
  entity	*newentity;	/* Used for cloning entities. */

/* Note that checks are performed in the ga_population_clone_empty() function. */

/*
 * Clone the population data.
 */
  newpop = ga_population_clone_empty(pop);

/*
 * Clone each of the constituent entities.
 */
  for (i=0; i<pop->size; i++)
    {
    newentity = ga_get_free_entity(newpop);
    ga_entity_copy(newpop, newentity, pop->entity_iarray[i]);
    }

  return newpop;
  }


/**********************************************************************
  ga_get_num_populations()
  synopsis:	Gets the number of populations.
  parameters:	none
  return:	int	number of populations, -1 for undefined table.
  last updated: 15 Aug 2002
 **********************************************************************/

int ga_get_num_populations(void)
  {
  int	num=-1;

  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    num = table_count_items(pop_table);
    }
  THREAD_UNLOCK(pop_table_lock);

  return num;
  }


/**********************************************************************
  ga_get_population_from_id()
  synopsis:	Get population pointer from its internal id.
  parameters:	unsigned int	id for population.
  return:	int
  last updated: 15 Aug 2002
 **********************************************************************/

population *ga_get_population_from_id(unsigned int id)
  {
  population	*pop=NULL;	/* The population pointer to return. */

  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    pop = (population *) table_get_data(pop_table, id);
    }
  THREAD_UNLOCK(pop_table_lock);

  return pop;
  }


/**********************************************************************
  ga_get_population_id()
  synopsis:	Get population's internal id from its pointer.
  parameters:	population	population pointer to lookup.
  return:	unsigned int	internal id for population (or -1 for no match).
  last updated: 15 Aug 2002
 **********************************************************************/

unsigned int ga_get_population_id(population *pop)
  {
  unsigned int	id=TABLE_ERROR_INDEX;	/* Internal population id. */

  THREAD_LOCK(pop_table_lock);
  if (pop_table && pop)
    {
    id = table_lookup_index(pop_table, (vpointer) pop);
    }
  THREAD_UNLOCK(pop_table_lock);

  return id;
  }


/**********************************************************************
  ga_get_all_population_ids()
  synopsis:	Get array of internal ids for all currently
		allocated populations.  The returned array needs to
		be deallocated by the caller.
  parameters:	none
  return:	unsigned int*	array of population ids (or NULL)
  last updated: 15 Aug 2002
 **********************************************************************/

unsigned int *ga_get_all_population_ids(void)
  {
  unsigned int	*ids=NULL;	/* Array of ids. */

  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    ids = table_get_index_all(pop_table);
    }
  THREAD_UNLOCK(pop_table_lock);

  return ids;
  }


/**********************************************************************
  ga_get_all_populations()
  synopsis:	Get array of all currently allocated populations.  The
		returned array needs to be deallocated by the caller.
  parameters:	none
  return:	population**	array of population pointers
  last updated: 15 Aug 2002
 **********************************************************************/

population **ga_get_all_populations(void)
  {
  population	**pops=NULL;	/* Array of all population pointers. */

  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    pops = (population **) table_get_data_all(pop_table);
    }
  THREAD_UNLOCK(pop_table_lock);

  return pops;
  }


/**********************************************************************
  ga_entity_seed()
  synopsis:	Fills a population structure with genes.  Defined in
		a user-specified function.
  parameters:	population *	The entity's population.
		entity *	The entity.
  return:	boolean success.
  last updated:	28/02/01
 **********************************************************************/

boolean ga_entity_seed(population *pop, entity *adam)
  {

  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !pop->seed ) die("Population seeding function is not defined.");

  return pop->seed(pop, adam);
  }


/**********************************************************************
  ga_population_seed()
  synopsis:	Fills all entities in a population structure with
		genes from a user-specified function.
  parameters:	none
  return:	boolean success.
  last updated: 28/02/01
 **********************************************************************/

boolean ga_population_seed(population *pop)
  {
  int		i;		/* Loop variables. */
  entity	*adam;		/* New population member. */

  plog(LOG_DEBUG, "Population seeding by user-defined genesis.");

  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !pop->seed ) die("Population seeding function is not defined.");

  for (i=0; i<pop->stable_size; i++)
    {
    adam = ga_get_free_entity(pop);
    pop->seed(pop, adam);
    }

  return TRUE;
  }


/**********************************************************************
  ga_population_seed_soup()
  synopsis:	Seed a population structure with starting genes from
		a previously created soup file.
  parameters:
  return:
  last updated: 06/07/00
 **********************************************************************/

boolean ga_population_seed_soup(population *pop, const char *fname)
  {
#if 0
  int		i, j, k;	/* Loop variables */
  entity	*this_entity;	/* Current population member */
  quaternion	q;		/* Quaternion */
  FILE          *fp;		/* File ptr */
  char          *line=NULL;	/* Line buffer */
  int           line_len;	/* Current buffer size */
  molstruct     *mol;		/* Current molstruct structure */
#endif

  plog(LOG_DEBUG, "Population seeding by reading soup file.");
  plog(LOG_FIXME, "Code incomplete.");

  if ( !fname ) die("Null pointer to filename passed.");
  if ( !pop ) die("Null pointer to population structure passed.");

#if 0
/*
 * Open soup file
 */
  FILE          *fp;		/* File ptr */
  if((fp=fopen(fname,"r"))==NULL)
    dief("Unable to open SOUP file \"%s\".", fname);
#endif


  return TRUE;
  }


/**********************************************************************
  ga_write_soup()
  synopsis:	Writes a soup file based on the current gene pool.
  parameters:
  return:
  last updated: 06/07/00
 **********************************************************************/

boolean ga_write_soup(population *pop)
  {

  plog(LOG_DEBUG, "Writing soup file.");
  plog(LOG_FIXME, "Code incomplete.");

  return TRUE;
  }


/**********************************************************************
  gaul_write_entity()
  synopsis:	Writes an entity to a filepointer using a binary format.
		Note: Currently does not (and probably can not) store
		any of the userdata.
  parameters:
  return:
  last updated: 29 May 2002
 **********************************************************************/

static void gaul_write_entity(FILE *fp, population *pop, entity *entity)
  {
  byte		*buffer=NULL;		/* Buffer for genetic data. */
  unsigned int	len, max_len=0;		/* Length of buffer. */

  fwrite(&(entity->fitness), sizeof(double), 1, fp);
  len = (int) pop->chromosome_to_bytes(pop, entity, &buffer, &max_len);
  fwrite(&len, sizeof(unsigned int), 1, fp);
  fwrite(buffer, sizeof(byte), len, fp);

  if (max_len!=0) s_free(buffer);

  return;
  }


/**********************************************************************
  gaul_read_entity()
  synopsis:	Reads an entity from a filepointer using a binary
		format.
  parameters:	FILE *fp	File handle from which to read entity.
		population *pop	Population to add new entity to.
  return:	entity*	An entity as read from disk.
  last updated: 30 May 2002
 **********************************************************************/

static entity *gaul_read_entity(FILE *fp, population *pop)
  {
  byte		*buffer=NULL;	/* Buffer for genetic data. */
  unsigned int	len;		/* Length of buffer. */
  entity	*entity;	/* New entity read from disk. */

  entity = ga_get_free_entity(pop);

  fread(&(entity->fitness), sizeof(double), 1, fp);
  fread(&len, sizeof(unsigned int), 1, fp);
  buffer = s_malloc(sizeof(byte)*len);
  fread(buffer, sizeof(byte), (size_t) len, fp);
  pop->chromosome_from_bytes(pop, entity, buffer);

  s_free(buffer);

  return entity;
  }


/**********************************************************************
  ga_funclookup_ptr_to_id()
  synopsis:     Assign a unique id to a callback function for
		population disk format from its pointer.
  parameters:
  return:
  last updated: 10 Apr 2003
 **********************************************************************/

int ga_funclookup_ptr_to_id(void *func)
  {
  int	id=1;	/* Index into lookup table. */

  if ( !func ) return 0;

  while (lookup[id].func_ptr != NULL &&
         func != lookup[id].func_ptr)
    id++;

#if GAUL_DEBUG>2
  printf("Function id is %d\n", id);
#endif

  return lookup[id].func_ptr!=NULL?id:-1;
  }


/**********************************************************************
  ga_funclookup_label_to_id()
  synopsis:     Assign a unique id to a callback function for
		population disk format from its label.
  parameters:
  return:
  last updated: 10 Apr 2003
 **********************************************************************/

int ga_funclookup_label_to_id(char *funcname)
  {
  int	id=1;	/* Index into lookup table. */

  if ( !funcname ) return 0;

  while (lookup[id].funcname != NULL &&
         strcmp(funcname, lookup[id].funcname) != 0)
    id++;

#if GAUL_DEBUG>2
  printf("Function id is %d\n", id);
#endif

  return lookup[id].func_ptr!=NULL?id:-1;
  }


/**********************************************************************
  ga_funclookup_label_to_ptr()
  synopsis:     Return the pointer to a callback function
		from its label.
  parameters:
  return:
  last updated: 10 Apr 2003
 **********************************************************************/

void *ga_funclookup_label_to_ptr(char *funcname)
  {
  int	id=1;	/* Index into lookup table. */

  if ( !funcname ) return 0;

  while (lookup[id].funcname != NULL &&
         strcmp(funcname, lookup[id].funcname) != 0)
    id++;

#if GAUL_DEBUG>2
  printf("Function id is %d\n", id);
#endif

  return lookup[id].func_ptr;
  }


/**********************************************************************
  ga_funclookup_id_to_ptr()
  synopsis:     Returns the pointer to a function from its unique id.
  parameters:
  return:
  last updated: 10 Apr 2003
 **********************************************************************/

void *ga_funclookup_id_to_ptr(int id)
  {

#if GAUL_DEBUG>2
  printf("Looking for function with id %d\n", id);
#endif

  return (id<0)?NULL:lookup[id].func_ptr;
  }


/**********************************************************************
  ga_funclookup_id_to_label()
  synopsis:     Returns the label for a function from its unique id.
  parameters:
  return:
  last updated: 10 Apr 2003
 **********************************************************************/

char *ga_funclookup_id_to_label(int id)
  {

#if GAUL_DEBUG>2
  printf("Looking for function with id %d\n", id);
#endif

  return (id<0)?NULL:lookup[id].funcname;
  }


/**********************************************************************
  ga_population_write()
  synopsis:	Writes entire population and it's genetic data to disk,
		using a binary format.
		Note: Currently does not (and probably can not) store
		any of the userdata.
  parameters:
  return:
  last updated: 29 May 2002
 **********************************************************************/

#define BUFFER_LEN	1024

boolean ga_population_write(population *pop, char *fname)
  {
  FILE          *fp;			/* File handle. */
  int		i;			/* Loop variables. */
  char		buffer[BUFFER_LEN];	/* String buffer. */
  int		id[18];			/* Array of hook indices. */
  int		count=0;		/* Number of unrecognised hook functions. */
  char		*format_str="FORMAT: GAUL POPULATION 002";	/* Format tag. */

/* Checks. */
  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !fname ) die("Null pointer to filename passed.");

/*
 * Open output file.
 */
  if( !(fp=fopen(fname,"w")) )
    dief("Unable to open population file \"%s\" for output.", fname);

/*
 * Program info.
 */
  fwrite(format_str, sizeof(char), strlen(format_str), fp);
  for (i=0; i<64; i++) buffer[i]='\0';
  snprintf(buffer, 64, "%s %s", VERSION_STRING, BUILD_DATE_STRING);
  fwrite(buffer, sizeof(char), 64, fp);

/*
 * Population info.
 */
  fwrite(&(pop->size), sizeof(int), 1, fp);
  fwrite(&(pop->stable_size), sizeof(int), 1, fp);
  fwrite(&(pop->num_chromosomes), sizeof(int), 1, fp);
  fwrite(&(pop->len_chromosomes), sizeof(int), 1, fp);

/*
 * GA parameters.
 */
  fwrite(&(pop->crossover_ratio), sizeof(double), 1, fp);
  fwrite(&(pop->mutation_ratio), sizeof(double), 1, fp);
  fwrite(&(pop->migration_ratio), sizeof(double), 1, fp);
  fwrite(&(pop->scheme), sizeof(int), 1, fp);
  fwrite(&(pop->elitism), sizeof(int), 1, fp);

  fwrite(&(pop->island), sizeof(int), 1, fp);

/*
 * Callback handling.  Note that user-implemented functions currently
 * can't be handled in these files.
 * id = -1 - Unknown, external function.
 * id = 0  - NULL function.
 * id > 0  - GAUL defined function.
 */
  id[0] = ga_funclookup_ptr_to_id((void *)pop->generation_hook);
  id[1] = ga_funclookup_ptr_to_id((void *)pop->iteration_hook);

  /*ga_funclookup_ptr_to_id((void *)pop->data_destructor);*/
  /*ga_funclookup_ptr_to_id((void *)pop->data_ref_incrementor);*/
  /* GAUL doesn't define any functions for either of these. */
  if (pop->data_destructor)
    id[2] = -1;
  else
    id[2] = 0;

  if (pop->data_ref_incrementor)
    id[3] = -1;
  else
    id[3] = 0;

  id[4] = ga_funclookup_ptr_to_id((void *)pop->chromosome_constructor);
  id[5] = ga_funclookup_ptr_to_id((void *)pop->chromosome_destructor);
  id[6] = ga_funclookup_ptr_to_id((void *)pop->chromosome_replicate);
  id[7] = ga_funclookup_ptr_to_id((void *)pop->chromosome_to_bytes);
  id[8] = ga_funclookup_ptr_to_id((void *)pop->chromosome_from_bytes);
  id[9] = ga_funclookup_ptr_to_id((void *)pop->chromosome_to_string);

  id[10] = ga_funclookup_ptr_to_id((void *)pop->evaluate);
  id[11] = ga_funclookup_ptr_to_id((void *)pop->seed);
  id[12] = ga_funclookup_ptr_to_id((void *)pop->adapt);
  id[13] = ga_funclookup_ptr_to_id((void *)pop->select_one);
  id[14] = ga_funclookup_ptr_to_id((void *)pop->select_two);
  id[15] = ga_funclookup_ptr_to_id((void *)pop->mutate);
  id[16] = ga_funclookup_ptr_to_id((void *)pop->crossover);
  id[17] = ga_funclookup_ptr_to_id((void *)pop->replace);

  fwrite(id, sizeof(int), 18, fp);

/*
 * Warn user of any unhandled data.
 */
  for (i=0; i<18; i++)
    if (id[i] == -1) count++;

  if (count>0)
    plog(LOG_NORMAL, "Unable to handle %d hook function%sspecified in population structure.", count, count==1?" ":"s ");

/*
 * Entity info.
 */
  for (i=0; i<pop->size; i++)
    {
    gaul_write_entity(fp, pop, pop->entity_iarray[i]);
    }

/*
 * Footer info.
 */
  fwrite("END", sizeof(char), 4, fp); 

/*
 * Close file.
 */
  fclose(fp);

  return TRUE;
  }


/**********************************************************************
  ga_population_read()
  synopsis:	Reads entire population and its genetic data back
		from disk.   Some things can't be restored.  See
		ga_population_write() for details.
		Compatibility version.
  parameters:	char *fname		Filename to read from.
  return:	population *pop		New population structure.
  last updated: 07 Nov 2002
 **********************************************************************/

static population *ga_population_read_001(char *fname)
  {
  population	*pop=NULL;		/* New population structure. */
  FILE          *fp;			/* File handle. */
  int		i;			/* Loop variables. */
  char		buffer[BUFFER_LEN];	/* String buffer. */
  int		id[18];			/* Array of hook indices. */
  int		count=0;		/* Number of unrecognised hook functions. */
  char		*format_str="FORMAT: GAUL POPULATION 001";	/* Format tag. */
  char		format_str_in[32]="";	/* Input format tag. (Empty initialiser to avoid valgrind warning...) */
  int		size, stable_size, num_chromosomes, len_chromosomes;	/* Input data. */

/* Checks. */
  if ( !fname ) die("Null pointer to filename passed.");

/*
 * Open output file.
 */
  if( !(fp=fopen(fname,"r")) )
    dief("Unable to open population file \"%s\" for input.", fname);

/*
 * Program info.
 */
  fread(format_str_in, sizeof(char), strlen(format_str), fp);
  if (strcmp(format_str, format_str_in)!=0)
    die("Incompatible format for population file.");

  fread(buffer, sizeof(char), 64, fp);	/* Presently ignored. */

/*
 * Population info.
 */
  fread(&size, sizeof(int), 1, fp);
  fread(&stable_size, sizeof(int), 1, fp);
  fread(&num_chromosomes, sizeof(int), 1, fp);
  fread(&len_chromosomes, sizeof(int), 1, fp);

/*
 * Allocate a new population structure.
 */
  pop = ga_population_new(stable_size, num_chromosomes, len_chromosomes);

/*
 * Got a population structure?
 */
  if ( !pop ) die("Unable to allocate population structure.");

/*
 * GA parameters.
 */
  fread(&(pop->crossover_ratio), sizeof(double), 1, fp);
  fread(&(pop->mutation_ratio), sizeof(double), 1, fp);
  fread(&(pop->migration_ratio), sizeof(double), 1, fp);
  fread(&(pop->scheme), sizeof(int), 1, fp);
  fread(&(pop->elitism), sizeof(int), 1, fp);

/*
 * Callback handling.  Note that user-implemented functions currently
 * can't be handled in these files.
 * id = -1 - Unknown, external function.
 * id = 0  - NULL function.
 * id > 0  - GAUL defined function.
 */
  fread(id, sizeof(int), 18, fp);

  pop->generation_hook        = (GAgeneration_hook)  ga_funclookup_id_to_ptr(id[0]);
  pop->iteration_hook         = (GAiteration_hook)   ga_funclookup_id_to_ptr(id[1]);

  pop->data_destructor        = (GAdata_destructor)      ga_funclookup_id_to_ptr(id[2]);
  pop->data_ref_incrementor   = (GAdata_ref_incrementor) ga_funclookup_id_to_ptr(id[3]);

  pop->chromosome_constructor = (GAchromosome_constructor) ga_funclookup_id_to_ptr(id[4]);
  pop->chromosome_destructor  = (GAchromosome_destructor)  ga_funclookup_id_to_ptr(id[5]);
  pop->chromosome_replicate   = (GAchromosome_replicate)   ga_funclookup_id_to_ptr(id[6]);
  pop->chromosome_to_bytes    = (GAchromosome_to_bytes)    ga_funclookup_id_to_ptr(id[7]);
  pop->chromosome_from_bytes  = (GAchromosome_from_bytes)  ga_funclookup_id_to_ptr(id[8]);
  pop->chromosome_to_string   = (GAchromosome_to_string)   ga_funclookup_id_to_ptr(id[9]);

  pop->evaluate               = (GAevaluate)       ga_funclookup_id_to_ptr(id[10]);
  pop->seed                   = (GAseed)           ga_funclookup_id_to_ptr(id[11]);
  pop->adapt                  = (GAadapt)          ga_funclookup_id_to_ptr(id[12]);
  pop->select_one             = (GAselect_one)     ga_funclookup_id_to_ptr(id[13]);
  pop->select_two             = (GAselect_two)     ga_funclookup_id_to_ptr(id[14]);
  pop->mutate                 = (GAmutate)         ga_funclookup_id_to_ptr(id[15]);
  pop->crossover              = (GAcrossover)      ga_funclookup_id_to_ptr(id[16]);
  pop->replace                = (GAreplace)        ga_funclookup_id_to_ptr(id[17]);

/*
 * Warn user of any unhandled data.
 */
  for (i=0; i<18; i++)
    if (id[i] == -1) count++;

  if (count>0)
    plog(LOG_NORMAL, "Unable to handle %d hook function%sspecified in population structure.", count, count==1?" ":"s ");

/*
 * Entity info.
 */
  for (i=0; i<size; i++)
    {
    gaul_read_entity(fp, pop);
    }

/*
 * Footer info.
 */
  fread(buffer, sizeof(char), 4, fp); 
  if (strcmp("END", buffer)!=0) die("Corrupt population file?");

/*
 * Close file.
 */
  fclose(fp);

  plog(LOG_DEBUG, "Have read %d entities into population.", pop->size);

  return pop;
  }


/**********************************************************************
  ga_population_read()
  synopsis:	Reads entire population and it's genetic data back
		from disk.   Some things can't be restored.  See
		ga_population_write() for details.
  parameters:	char *fname		Filename to read from.
  return:	population *pop		New population structure.
  last updated: 07 Nov 2002
 **********************************************************************/

population *ga_population_read(char *fname)
  {
  population	*pop=NULL;		/* New population structure. */
  FILE          *fp;			/* File handle. */
  int		i;			/* Loop variables. */
  char		buffer[BUFFER_LEN];	/* String buffer. */
  int		id[18];			/* Array of hook indices. */
  int		count=0;		/* Number of unrecognised hook functions. */
  char		*format_str="FORMAT: GAUL POPULATION 002";	/* Format tag. */
  char		format_str_in[32]="";	/* Input format tag. (Empty initialiser to avoid valgrind warning...) */
  int		size, stable_size, num_chromosomes, len_chromosomes;	/* Input data. */

/* Checks. */
  if ( !fname ) die("Null pointer to filename passed.");

/*
 * Open output file.
 */
  if( !(fp=fopen(fname,"r")) )
    dief("Unable to open population file \"%s\" for input.", fname);

/*
 * Program info.
 */
  fread(format_str_in, sizeof(char), strlen(format_str), fp);
  if (strcmp(format_str, format_str_in)!=0)
    {
    fclose(fp);
    plog(LOG_WARNING, "Old format for population file. (Pre-002)");
    return ga_population_read_001(fname);
    }

  fread(buffer, sizeof(char), 64, fp);	/* Presently ignored. */

/*
 * Population info.
 */
  fread(&size, sizeof(int), 1, fp);
  fread(&stable_size, sizeof(int), 1, fp);
  fread(&num_chromosomes, sizeof(int), 1, fp);
  fread(&len_chromosomes, sizeof(int), 1, fp);

/*
 * Allocate a new population structure.
 */
  pop = ga_population_new(stable_size, num_chromosomes, len_chromosomes);

/*
 * Got a population structure?
 */
  if ( !pop ) die("Unable to allocate population structure.");

/*
 * GA parameters.
 */
  fread(&(pop->crossover_ratio), sizeof(double), 1, fp);
  fread(&(pop->mutation_ratio), sizeof(double), 1, fp);
  fread(&(pop->migration_ratio), sizeof(double), 1, fp);
  fread(&(pop->scheme), sizeof(int), 1, fp);
  fread(&(pop->elitism), sizeof(int), 1, fp);
  fread(&(pop->island), sizeof(int), 1, fp);

/*
 * Callback handling.  Note that user-implemented functions currently
 * can't be handled in these files.
 * id = -1 - Unknown, external function.
 * id = 0  - NULL function.
 * id > 0  - GAUL defined function.
 */
  fread(id, sizeof(int), 18, fp);

  pop->generation_hook        = (GAgeneration_hook)  ga_funclookup_id_to_ptr(id[0]);
  pop->iteration_hook         = (GAiteration_hook)   ga_funclookup_id_to_ptr(id[1]);

  pop->data_destructor        = (GAdata_destructor)      ga_funclookup_id_to_ptr(id[2]);
  pop->data_ref_incrementor   = (GAdata_ref_incrementor) ga_funclookup_id_to_ptr(id[3]);

  pop->chromosome_constructor = (GAchromosome_constructor) ga_funclookup_id_to_ptr(id[4]);
  pop->chromosome_destructor  = (GAchromosome_destructor)  ga_funclookup_id_to_ptr(id[5]);
  pop->chromosome_replicate   = (GAchromosome_replicate)   ga_funclookup_id_to_ptr(id[6]);
  pop->chromosome_to_bytes    = (GAchromosome_to_bytes)    ga_funclookup_id_to_ptr(id[7]);
  pop->chromosome_from_bytes  = (GAchromosome_from_bytes)  ga_funclookup_id_to_ptr(id[8]);
  pop->chromosome_to_string   = (GAchromosome_to_string)   ga_funclookup_id_to_ptr(id[9]);

  pop->evaluate               = (GAevaluate)       ga_funclookup_id_to_ptr(id[10]);
  pop->seed                   = (GAseed)           ga_funclookup_id_to_ptr(id[11]);
  pop->adapt                  = (GAadapt)          ga_funclookup_id_to_ptr(id[12]);
  pop->select_one             = (GAselect_one)     ga_funclookup_id_to_ptr(id[13]);
  pop->select_two             = (GAselect_two)     ga_funclookup_id_to_ptr(id[14]);
  pop->mutate                 = (GAmutate)         ga_funclookup_id_to_ptr(id[15]);
  pop->crossover              = (GAcrossover)      ga_funclookup_id_to_ptr(id[16]);
  pop->replace                = (GAreplace)        ga_funclookup_id_to_ptr(id[17]);

/*
 * Warn user of any unhandled data.
 */
  for (i=0; i<18; i++)
    if (id[i] == -1) count++;

  if (count>0)
    plog(LOG_NORMAL, "Unable to handle %d hook function%sspecified in population structure.", count, count==1?" ":"s ");

/*
 * Entity info.
 */
  for (i=0; i<size; i++)
    {
    gaul_read_entity(fp, pop);
    }

/*
 * Footer info.
 */
  fread(buffer, sizeof(char), 4, fp); 
  if (strcmp("END", buffer)!=0) die("Corrupt population file?");

/*
 * Close file.
 */
  fclose(fp);

  plog(LOG_DEBUG, "Have read %d entities into population.", pop->size);

  return pop;
  }


/**********************************************************************
  ga_entity_write()
  synopsis:	Write an entity to disk.
		Note: Currently does not (and probably can not) store
		any of the userdata.
  parameters:	population *pop
		entity *entity
		char *fname
  return:	TRUE
  last updated: 07 Nov 2002
 **********************************************************************/

boolean ga_entity_write(population *pop, entity *entity, char *fname)
  {
  int		i;			/* Loop variable. */
  char		buffer[BUFFER_LEN];	/* String buffer. */
  char		*format_str="FORMAT: GAUL ENTITY 001";	/* Format tag. */
  FILE		*fp;			/* Filehandle. */

/* Checks. */
  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !entity ) die("Null pointer to entity structure passed.");
  if ( !fname ) die("Null pointer to filename passed.");

/*
 * Open output file.
 */
  if( !(fp=fopen(fname,"w")) )
    dief("Unable to open entity file \"%s\" for output.", fname);

/*
 * Write stuff.
 */
  fwrite(format_str, sizeof(char), strlen(format_str), fp);
  for (i=0; i<64; i++) buffer[i]='\0';
  snprintf(buffer, 64, "%s %s", VERSION_STRING, BUILD_DATE_STRING);
  fwrite(buffer, sizeof(char), 64, fp);

  gaul_write_entity(fp, pop, entity);

  fwrite("END", sizeof(char), 4, fp); 

  fclose(fp);

  return TRUE;
  }


/**********************************************************************
  ga_entity_read()
  synopsis:	Read an entity from disk.
		Note: Currently does not (and probably can not) store
		any of the userdata.
  parameters:	population *pop
		entity *entity
		char *fname
  return:	TRUE
  last updated: 30 May 2002
 **********************************************************************/

entity *ga_entity_read(population *pop, char *fname)
  {
  char		buffer[BUFFER_LEN];	/* String buffer. */
  char		*format_str="FORMAT: GAUL ENTITY 001";	/* Format tag. */
  char		format_str_in[32];	/* Input format tag. */
  FILE		*fp;			/* Filehandle. */
  entity	*entity;		/* Input entity. */

/* Checks. */
  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !fname ) die("Null pointer to filename passed.");

/*
 * Open output file.
 */
  if( !(fp=fopen(fname,"w")) )
    dief("Unable to open entity file \"%s\" for output.", fname);

/*
 * Read stuff.
 */
  fread(format_str_in, sizeof(char), strlen(format_str), fp);
  if (strcmp(format_str, format_str_in)!=0) die("Incorrect format for entity file.");
  fread(buffer, sizeof(char), 64, fp);	/* Ignored. */

  entity = gaul_read_entity(fp, pop);

  fread(buffer, sizeof(char), 4, fp); 
  if (strcmp("END", buffer)!=0) die("Corrupt population file?");

  fclose(fp);

  return entity;
  }


/**********************************************************************
  ga_entity_evaluate()
  synopsis:	Score a single entity.
  parameters:	population *pop
		entity *entity
  return:	double			the fitness
  last updated: 06 Feb 2002
 **********************************************************************/

double ga_entity_evaluate(population *pop, entity *entity)
  {

  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !entity ) die("Null pointer to entity structure passed.");
  if ( !pop->evaluate ) die("Evaluation callback not defined.");

  pop->evaluate(pop, entity);

  return entity->fitness;
  }


/**********************************************************************
  ga_population_score_and_sort()
  synopsis:	Score and sort entire population.  This is probably
		a good idea after changing the fitness function!
		Note: remember to define the callback functions first.
  parameters:
  return:
  last updated: 28/02/01
 **********************************************************************/

boolean ga_population_score_and_sort(population *pop)
  {
  int		i;		/* Loop variable over all entities. */
  double	origfitness;	/* Stored fitness value. */

/* Checks. */
  if ( !pop ) die("Null pointer to population structure passed.");

/*
 * Score and sort all of the population members.
 *
 * Note that this will (potentially) use a huge amount of memory more
 * than the original population data if the userdata hasn't been maintained.
 * Each chromosome is decoded separately, whereas originally many
 * degenerate chromosomes would share their userdata elements.
 */
  for (i=0; i<pop->size; i++)
    {
    origfitness = pop->entity_iarray[i]->fitness;
    pop->evaluate(pop, pop->entity_iarray[i]);

#if GA_DEBUG>2
    if (origfitness != pop->entity_iarray[i]->fitness)
      plog(LOG_NORMAL,
           "Recalculated fitness %f doesn't match stored fitness %f for entity %d.",
           pop->entity_iarray[i]->fitness, origfitness, i);
#endif
    }

  sort_population(pop);

  return TRUE;
  }


/**********************************************************************
  ga_population_sort()
  synopsis:	Sort entire population (i.e. ensure that the entities
		are correctly ordered in ranking array -- currently
		rank is defined only by fitness).
		Note: remember to define the callback functions first.
  parameters:
  return:
  last updated: 30 May 2002
 **********************************************************************/

boolean ga_population_sort(population *pop)
  {

/* Checks. */
  if ( !pop ) die("Null pointer to population structure passed.");

  sort_population(pop);

  return TRUE;
  }


#if 0
FIXME: The following 3 functions need to be fixed for the new absracted chromosome types.
/**********************************************************************
  ga_population_convergence_genotypes()
  synopsis:	Determine ratio of converged genotypes in population.
  parameters:
  return:
  last updated: 31/05/01
 **********************************************************************/

double ga_population_convergence_genotypes( population *pop )
  {
  int		i, j;		/* Loop over pairs of entities. */
  int		total=0, converged=0;	/* Number of comparisons, matches. */

  if ( !pop ) die("Null pointer to population structure passed.");
  if (pop->size < 1) die("Pointer to empty population structure passed.");

  for (i=1; i<pop->size; i++)
    {
    for (j=0; j<i; j++)
      {
      if (ga_compare_genome(pop, pop->entity_iarray[i], pop->entity_iarray[j]))
        converged++;
      total++;
      }
    }

  return (double) converged/total;
  }


/**********************************************************************
  ga_population_convergence_chromosomes()
  synopsis:	Determine ratio of converged chromosomes in population.
  parameters:
  return:
  last updated: 31/05/01
 **********************************************************************/

double ga_population_convergence_chromosomes( population *pop )
  {
  int		i, j;		/* Loop over pairs of entities. */
  int		k;		/* Loop over chromosomes. */
  int		total=0, converged=0;	/* Number of comparisons, matches. */

  if ( !pop ) die("Null pointer to population structure passed.");
  if (pop->size < 1) die("Pointer to empty population structure passed.");

  for (i=1; i<pop->size; i++)
    {
    for (j=0; j<i; j++)
      {
      for (k=0; k<pop->num_chromosomes; k++)
        {
/* FIXME: Not totally effiecient: */
        if (ga_count_match_alleles( pop->len_chromosomes,
                                    pop->entity_iarray[i]->chromosome[k],
                                    pop->entity_iarray[j]->chromosome[k] ) == pop->len_chromosomes)
          converged++;
        total++;
        }
      }
    }

  return (double) converged/total;
  }


/**********************************************************************
  ga_population_convergence_alleles()
  synopsis:	Determine ratio of converged alleles in population.
  parameters:
  return:
  last updated: 31/05/01
 **********************************************************************/

double ga_population_convergence_alleles( population *pop )
  {
  int		i, j;		/* Loop over pairs of entities. */
  int		k;		/* Loop over chromosomes. */
  int		total=0, converged=0;	/* Number of comparisons, matches. */

  if ( !pop ) die("Null pointer to population structure passed.");
  if (pop->size < 1) die("Pointer to empty population structure passed.");

  for (i=1; i<pop->size; i++)
    {
    for (j=0; j<i; j++)
      {
      for (k=0; k<pop->num_chromosomes; k++)
        {
        converged+=ga_count_match_alleles( pop->len_chromosomes,
                                           pop->entity_iarray[i]->chromosome[k],
                                           pop->entity_iarray[j]->chromosome[k] );
        total+=pop->len_chromosomes;
        }
      }
    }

  return (double) converged/total;
  }
#endif


/**********************************************************************
  ga_get_entity_rank()
  synopsis:	Gets an entity's rank (subscript into entity_iarray of
		the population).  This is not necessarily the fitness
		rank unless the population has been sorted.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

int ga_get_entity_rank(population *pop, entity *e)
  {
  int	rank=0;		/* The rank. */

  while (rank < pop->size)
    {
    if (pop->entity_iarray[rank] == e) return rank;
    rank++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_rank_from_id()
  synopsis:	Gets an entity's rank (subscript into entity_iarray of
		the population).  This is not necessarily the fitness
		rank unless the population has been sorted.
  parameters:
  return:
  last updated: 18 Mar 2002
 **********************************************************************/

int ga_get_entity_rank_from_id(population *pop, int id)
  {
  int	rank=0;		/* The rank. */

  while (rank < pop->size)
    {
    if (pop->entity_iarray[rank] == pop->entity_array[id]) return rank;
    rank++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_id_from_rank()
  synopsis:	Gets an entity's id from its rank.
  parameters:
  return:
  last updated: 18 Mar 2002
 **********************************************************************/

int ga_get_entity_id_from_rank(population *pop, int rank)
  {
  int	id=0;		/* The entity's index. */

  while (id < pop->max_size)
    {
    if (pop->entity_array[id] == pop->entity_iarray[rank]) return id;
    id++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_id()
  synopsis:	Gets an entity's internal index.
  parameters:	population *pop
		entity *e
  return:	entity id
  last updated: 18 Mar 2002
 **********************************************************************/

int ga_get_entity_id(population *pop, entity *e)
  {
  int	id=0;	/* The index. */

  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !e ) die("Null pointer to entity structure passed.");

  while (id < pop->max_size)
    {
    if (pop->entity_array[id] == e) return id;
    id++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_from_id()
  synopsis:	Gets a pointer to an entity from it's internal index
		(subscript in the entity_array array).
  parameters:
  return:
  last updated: 29 Apr 2002
 **********************************************************************/

entity *ga_get_entity_from_id(population *pop, const unsigned int id)
  {
  if ( !pop ) die("Null pointer to population structure passed.");

  if (id>pop->max_size) return NULL;

  return pop->entity_array[id];
  }


/**********************************************************************
  ga_get_entity_from_rank()
  synopsis:	Gets a pointer to an entity from it's internal rank.
		(subscript into the entity_iarray buffer).
		Note that this only relates to fitness ranking if
		the population has been properly sorted.
		FIXME: Add some error checking.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

entity *ga_get_entity_from_rank(population *pop, const unsigned int rank)
  {
  return pop->entity_iarray[rank];
  }


/**********************************************************************
  ga_entity_setup()
  synopsis:	Allocates/prepares an entity structure for use.
		Chromosomes are allocated, but will contain garbage.
  parameters:
  return:
  last updated: 18 Mar 2002
 **********************************************************************/

boolean ga_entity_setup(population *pop, entity *joe)
  {

  if (!joe)
    die("Null pointer to entity structure passed.");
  if (!pop->chromosome_constructor)
    die("Chromosome constructor not defined.");

/* Allocate chromosome structures. */
  joe->chromosome = NULL;
  pop->chromosome_constructor(pop, joe);

/* Physical characteristics currently undefined. */
  joe->data=NULL;

/* No fitness evaluated yet. */
  joe->fitness=GA_MIN_FITNESS;

  return TRUE;
  }


/**********************************************************************
  ga_entity_dereference_by_rank()
  synopsis:	Marks an entity structure as unused.
		Deallocation is expensive.  It is better to re-use this
		memory.  So, that is what we do.
		Any contents of entities data field are freed.
		If rank is known, this is much quicker than the plain
		ga_entity_dereference() function.
		Note, no error checking in the interests of speed.
  parameters:
  return:
  last updated:	19 Mar 2002
 **********************************************************************/

boolean ga_entity_dereference_by_rank(population *pop, int rank)
  {
  int		i;	/* Loop variable over the indexed array. */
  entity	*dying=pop->entity_iarray[rank];	/* Dead entity. */

  if (!dying) die("Invalid entity rank");

/* Clear user data. */
  if (dying->data)
    {
    destruct_list(pop, dying->data);
    dying->data=NULL;
    }

/* Population size is one less now! */
  pop->size--;

/* Deallocate chromosomes. */
  if (dying->chromosome) pop->chromosome_destructor(pop, dying);

/* Update entity_iarray[], so there are no gaps! */
  for (i=rank; i<pop->size; i++)
    pop->entity_iarray[i] = pop->entity_iarray[i+1];

  pop->entity_iarray[pop->size] = NULL;

/* Release index. */
  i = ga_get_entity_id(pop, dying);
  pop->entity_array[ga_get_entity_id(pop, dying)] = NULL;

/* Release memory. */
  mem_chunk_free(pop->entity_chunk, dying);

/*  printf("ENTITY %d DEREFERENCED. New pop size = %d\n", i, pop->size);*/

  return TRUE;
  }


/**********************************************************************
  ga_entity_dereference_by_id()
  synopsis:	Marks an entity structure as unused.
		Deallocation is expensive.  It is better to re-use this
		memory.  So, that is what we do.
		Any contents of entities data field are freed.
		If rank is known, this is much quicker than the plain
		ga_entity_dereference() function, while this index
		based version is still almost as fast.
		Note, no error checking in the interests of speed.
  parameters:
  return:
  last updated:	19 Mar 2002
 **********************************************************************/

boolean ga_entity_dereference_by_id(population *pop, int id)
  {
  int		i;	/* Loop variable over the indexed array. */
  entity	*dying=pop->entity_array[id];	/* Dead entity. */

  if (!dying) die("Invalid entity index");

/* Clear user data. */
  if (dying->data)
    {
    destruct_list(pop, dying->data);
    dying->data=NULL;
    }

/* Population size is one less now! */
  pop->size--;

/* Update entity_iarray[], so there are no gaps! */
  for (i=ga_get_entity_rank(pop, dying); i<pop->size; i++)
    pop->entity_iarray[i] = pop->entity_iarray[i+1];

  pop->entity_iarray[pop->size] = NULL;

/* Deallocate chromosomes. */
  if (dying->chromosome) pop->chromosome_destructor(pop, dying);

/* Release index. */
  pop->entity_array[id] = NULL;

/* Release memory. */
  mem_chunk_free(pop->entity_chunk, dying);

/*  printf("ENTITY %d DEREFERENCED. New pop size = %d\n", id, pop->size);*/

  return TRUE;
  }


/**********************************************************************
  ga_entity_dereference()
  synopsis:	Marks an entity structure as unused.
		Deallocation is expensive.  It is better to re-use this
		memory.  So, that is what we do.
		Any contents of entities data field are freed.
		If rank is known, the above
		ga_entity_dereference_by_rank() or
		ga_entity_dereference_by_id() functions are much
		faster.
  parameters:
  return:
  last updated:	16/03/01
 **********************************************************************/

boolean ga_entity_dereference(population *pop, entity *dying)
  {
  return ga_entity_dereference_by_rank(pop, ga_get_entity_rank(pop, dying));
  }


/**********************************************************************
  ga_entity_clear_data()
  synopsis:	Clears some of the entity's data.  Safe if data doesn't
		exist anyway.
  parameters:
  return:
  last updated: 20/12/00
 **********************************************************************/

void ga_entity_clear_data(population *p, entity *entity, const int chromosome)
  {
  SLList	*tmplist;
  vpointer	data;		/* Data in item. */

  if (entity->data)
    {
    tmplist = slink_nth(entity->data, chromosome);
    if ( (data = slink_data(tmplist)) )
      {
      p->data_destructor(data);
      tmplist->data=NULL;
      }
    }

  return;
  }


/**********************************************************************
  ga_entity_blank()
  synopsis:	Clears the entity's data.
		Equivalent to an optimised ga_entity_dereference()
		followed by ga_get_free_entity().  It is much more
		preferable to use this fuction!
		Chromosomes are gaurenteed to be intact, but may be
		overwritten by user.
  parameters:
  return:
  last updated: 18/12/00
 **********************************************************************/

void ga_entity_blank(population *p, entity *entity)
  {
  if (entity->data)
    {
    destruct_list(p, entity->data);
    entity->data=NULL;
    }

  entity->fitness=GA_MIN_FITNESS;

/*  printf("ENTITY %d CLEARED.\n", ga_get_entity_id(p, entity));*/

  return;
  }


/**********************************************************************
  ga_get_free_entity()
  synopsis:	Returns pointer to an unused entity structure from the
		population's entity pool.  Increments population size
		too.
  parameters:	population *pop
  return:	entity *entity
  last updated: 18 Mar 2002
 **********************************************************************/

entity *ga_get_free_entity(population *pop)
  {
  int		new_max_size;	/* Increased maximum number of entities. */
  int		i;
  entity	*new;		/* Unused entity structure. */

/*
  plog(LOG_DEBUG, "Locating free entity structure.");
*/

/*
 * Do we have room for any new structures?
 */
  if (pop->max_size == (pop->size+1))
    {	/* No, so allocate some more space. */
    plog(LOG_VERBOSE, "No unused entities available -- allocating additional structures.");

    new_max_size = (pop->max_size * 3)/2;
    pop->entity_array = s_realloc(pop->entity_array, new_max_size*sizeof(entity*));
    pop->entity_iarray = s_realloc(pop->entity_iarray, new_max_size*sizeof(entity*));

    for (i=pop->max_size; i<new_max_size; i++)
      {
      pop->entity_array[i] = NULL;
      pop->entity_iarray[i] = NULL;
      }

    pop->max_size = new_max_size;
    pop->free_index = new_max_size-1;
    }

/* Find unused entity index. */
  while (pop->entity_array[pop->free_index]!=NULL)
    {
    if (pop->free_index == 0) pop->free_index=pop->max_size;
    pop->free_index--;
    }

/* Prepare it. */
  new = mem_chunk_alloc(pop->entity_chunk);
  pop->entity_array[pop->free_index] = new;
  ga_entity_setup(pop, new);

/* Store in lowest free slot in entity_iarray */
  pop->entity_iarray[pop->size] = new;

/* Population is bigger now! */
  pop->size++;

/*  printf("ENTITY %d ALLOCATED.\n", pop->free_index);*/

  return new;
  }


/**********************************************************************
  ga_copy_data()
  synopsis:	Copy one chromosome's portion of the data field of an
		entity structure to another entity structure.  (Copies
		the portion of the phenome relating to that chromosome)
		'Copies' NULL data safely.
		The destination chromosomes must be filled in order.
		If these entities are in differing populations, no
		problems will occur provided that the
		data_ref_incrementor callbacks are identical or at least
		compatible.
  parameters:
  return:
  last updated: 18/12/00
 **********************************************************************/

boolean ga_copy_data(population *pop, entity *dest, entity *src, const int chromosome)
  {
  vpointer	tmpdata=NULL;	/* Temporary pointer. */

  if ( !src || !(tmpdata = slink_nth_data(src->data, chromosome)) )
    {
    dest->data = slink_append(dest->data, NULL);
    }
  else
    {
    dest->data = slink_append(dest->data, tmpdata);
    pop->data_ref_incrementor(tmpdata);
    }

  return TRUE;
  }


/**********************************************************************
  ga_copy_chromosome()
  synopsis:	Copy one chromosome between entities.
		If these entities are in differing populations, no
		problems will occur provided that the chromosome
		datatypes match up.
  parameters:
  return:
  last updated: 18/12/00
 **********************************************************************/

static boolean ga_copy_chromosome( population *pop, entity *dest, entity *src,
                            const int chromosome )
  {

  pop->chromosome_replicate(pop, src, dest, chromosome);

  return TRUE;
  }


/**********************************************************************
  ga_entity_copy_all_chromosomes()
  synopsis:	Copy genetic data between entity structures.
		If these entities are in differing populations, no
		problems will occur provided that the chromosome
		properties are identical.
  parameters:
  return:
  last updated: 20/12/00
 **********************************************************************/

boolean ga_entity_copy_all_chromosomes(population *pop, entity *dest, entity *src)
  {
  int		i;		/* Loop variable over all chromosomes. */

  /* Checks */
  if (!dest || !src) die("Null pointer to entity structure passed");

/*
 * Ensure destination structure is not likely be already in use.
 */
  if (dest->data) die("Why does this entity already contain data?");

/*
 * Copy genetic data.
 */
  for (i=0; i<pop->num_chromosomes; i++)
    {
    ga_copy_data(pop, dest, src, i);		/* Phenome. */
    ga_copy_chromosome(pop, dest, src, i);	/* Genome. */
    }

  return TRUE;
  }


/**********************************************************************
  ga_entity_copy_chromosome()
  synopsis:	Copy chromosome and structural data between entity
		structures.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

boolean ga_entity_copy_chromosome(population *pop, entity *dest, entity *src, int chromo)
  {

/* Checks. */
  if (!dest || !src) die("Null pointer to entity structure passed");
  if (chromo<0 || chromo>=pop->num_chromosomes) die("Invalid chromosome number.");

/*
 * Ensure destination structure is not likely be already in use.
 */
  if (dest->data) die("Why does this entity already contain data?");

/*
 * Copy genetic and associated structural data (phenomic data).
 */
/*
  memcpy(dest->chromosome[chromo], src->chromosome[chromo],
           pop->len_chromosomes*sizeof(int));
*/
  ga_copy_data(pop, dest, src, chromo);
  ga_copy_chromosome(pop, dest, src, chromo);

  return TRUE;
  }


/**********************************************************************
  ga_entity_copy()
  synopsis:	Copy entire entity structure.  This is safe
		for copying between populations... provided that they
		are compatible.
  parameters:
  return:
  last updated:	22/01/01
 **********************************************************************/

boolean ga_entity_copy(population *pop, entity *dest, entity *src)
  {

  ga_entity_copy_all_chromosomes(pop, dest, src);
  dest->fitness = src->fitness;

  return TRUE;
  }


/**********************************************************************
  ga_entity_clone()
  synopsis:	Clone an entity structure.
		Safe for cloning into a different population, provided
		that the populations are compatible.
  parameters:	population	*pop	Population.
		entity	*parent		The original entity.
  return:	entity	*dolly		The new entity.
  last updated:	07/07/01
 **********************************************************************/

entity *ga_entity_clone(population *pop, entity *parent)
  {
  entity	*dolly;		/* The clone. */

  dolly = ga_get_free_entity(pop);

  ga_entity_copy_all_chromosomes(pop, dolly, parent);
  dolly->fitness = parent->fitness;

  return dolly;
  }


/**********************************************************************
  Network communication (population/entity migration) functions.
 **********************************************************************/

/**********************************************************************
  ga_population_send_by_mask()
  synopsis:	Send selected entities from a population to another
		processor.  Only fitness and chromosomes sent.
  parameters:
  return:
  last updated: 31 Jan 2002
 **********************************************************************/

void ga_population_send_by_mask( population *pop, int dest_node, int num_to_send, boolean *send_mask )
  {
  int		i;
  int		count=0;
  int		len=0;		/* Length of buffer to send. */
  unsigned int	max_len=0;
  byte		*buffer=NULL;

/*
 * Send number of entities.
 */
  mpi_send(&num_to_send, 1, MPI_TYPE_INT, dest_node, GA_TAG_NUMENTITIES);

/* 
 * Slight knudge to determine length of buffer.  Should have a more
 * elegant approach for this.
 * Sending this length here should not be required at all.
 */
  len = (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[0], &buffer, &max_len);
  mpi_send(&len, 1, MPI_TYPE_INT, dest_node, GA_TAG_ENTITYLEN);

/*
  printf("DEBUG: Node %d sending %d entities of length %d to %d\n",
           mpi_get_rank(), num_to_send, len, dest_node);
*/

/*
 * Send required entities individually.
 */
  for (i=0; i<pop->size && count<num_to_send; i++)
    {
    if (send_mask[i])
      {
/* printf("DEBUG: Node %d sending entity %d/%d (%d/%d) with fitness %f\n",
             mpi_get_rank(), count, num_to_send, i, pop->size, pop->entity_iarray[i]->fitness); */
      mpi_send(&(pop->entity_iarray[i]->fitness), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_ENTITYFITNESS);
      if (len != (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[i], &buffer, &max_len))
	die("Internal length mismatch");
      mpi_send(buffer, len, MPI_TYPE_BYTE, dest_node, GA_TAG_ENTITYCHROMOSOME);
      count++;
      }
    }

  if (count != num_to_send)
    die("Incorrect value for num_to_send");

/*
 * We only need to deallocate the buffer if it was allocated (i.e. if
 * the "chromosome_to_bytes" callback set max_len).
 */
  if (max_len!=0) s_free(buffer);

/*  printf("DEBUG: Node %d finished sending\n", mpi_get_rank());*/

  return;
  }


/**********************************************************************
  ga_population_send_every()
  synopsis:	Send all entities from a population to another
		processor.  Only fitness and chromosomes sent.
  parameters:
  return:
  last updated: 31 Jan 2002
 **********************************************************************/

void ga_population_send_every( population *pop, int dest_node )
  {
  int		i;
  int		len;			/* Length of buffer to send. */
  unsigned int	max_len=0;		/* Maximum length of buffer. */
  byte		*buffer=NULL;

  if ( !pop ) die("Null pointer to population structure passed.");

/*
 * Send number of entities.
 */
  mpi_send(&(pop->size), 1, MPI_TYPE_INT, dest_node, GA_TAG_NUMENTITIES);

/* 
 * Slight kludge to determine length of buffer.  Should have a more
 * elegant approach for this.
 * Sending this length here should not be required at all.
 */
  len = (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[0], &buffer, &max_len);
  mpi_send(&len, 1, MPI_TYPE_INT, dest_node, GA_TAG_ENTITYLEN);

/*
 * Send all entities individually.
 */
  for (i=0; i<pop->size; i++)
    {
    mpi_send(&(pop->entity_iarray[i]->fitness), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_ENTITYFITNESS);
    if (len != (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[i], &buffer, &max_len))
      die("Internal length mismatch");
    mpi_send(buffer, len, MPI_TYPE_BYTE, dest_node, GA_TAG_ENTITYCHROMOSOME);
    }

/*
 * We only need to deallocate the buffer if it was allocated (i.e. if
 * the "chromosome_to_bytes" callback set max_len).
 */
  if (max_len!=0) s_free(buffer);

  return;
  }


/**********************************************************************
  ga_population_append_receive()
  synopsis:	Recieve a set of entities from a population on another
		processor and append them to a current population.
		Only fitness and chromosomes received.
  parameters:
  return:
  last updated: 31 Jan 2002
 **********************************************************************/

void ga_population_append_receive( population *pop, int src_node )
  {
  int		i;
  int		len=0;			/* Length of buffer to receive. */
  byte		*buffer;		/* Receive buffer. */
  int		num_to_recv;		/* Number of entities to receive. */
  entity	*entity;		/* New entity. */

  if ( !pop ) die("Null pointer to population structure passed.");

/*
 * Get number of entities to receive and the length of each.
 * FIXME: This length data shouldn't be needed!
 */
  mpi_receive(&num_to_recv, 1, MPI_TYPE_INT, src_node, GA_TAG_NUMENTITIES);
  mpi_receive(&len, 1, MPI_TYPE_INT, src_node, GA_TAG_ENTITYLEN);

/*
  printf("DEBUG: Node %d anticipating %d entities of length %d from %d\n",
           mpi_get_rank(), num_to_recv, len, src_node);
*/

  if (num_to_recv>0)
    {
    buffer = s_malloc(len*sizeof(byte));

/*
 * Receive all entities individually.
 */
    for (i=0; i<num_to_recv; i++)
      {
      entity = ga_get_free_entity(pop);
      mpi_receive(&(entity->fitness), 1, MPI_TYPE_DOUBLE, src_node, GA_TAG_ENTITYFITNESS);
      mpi_receive(buffer, len, MPI_TYPE_BYTE, src_node, GA_TAG_ENTITYCHROMOSOME);
      pop->chromosome_from_bytes(pop, entity, buffer);
/*      printf("DEBUG: Node %d received entity %d/%d (%d) with fitness %f\n",
             mpi_get_rank(), i, num_to_recv, pop->size, entity->fitness);
 */
      }

    s_free(buffer);
    }

/*  printf("DEBUG: Node %d finished receiving\n", mpi_get_rank());*/

  return;
  }


/**********************************************************************
  ga_population_new_receive()
  synopsis:	Recieve a population structure (excluding actual
  		entities) from another processor.
		Note that the callbacks wiil need to be subsequently
		defined by the user.
  parameters:
  return:
  last updated: 24 May 2002
 **********************************************************************/

population *ga_population_new_receive( int src_node )
  {
  population *pop=NULL;

  plog(LOG_FIXME, "Function not fully implemented");
  mpi_receive(&(pop->stable_size), 1, MPI_TYPE_INT, src_node, GA_TAG_POPSTABLESIZE);
  mpi_receive(&(pop->crossover_ratio), 1, MPI_TYPE_DOUBLE, src_node, GA_TAG_POPCROSSOVER);
  mpi_receive(&(pop->mutation_ratio), 1, MPI_TYPE_DOUBLE, src_node, GA_TAG_POPMUTATION);
  mpi_receive(&(pop->migration_ratio), 1, MPI_TYPE_DOUBLE, src_node, GA_TAG_POPMIGRATION);

  return pop;
  }


/**********************************************************************
  ga_population_receive()
  synopsis:	Recieve a population structure (including actual
  		entities) from another processor.
  parameters:
  return:
  last updated: 24 Jan 2002
 **********************************************************************/

population *ga_population_receive( int src_node )
  {
  population *pop;

  pop = ga_population_new_receive( src_node );
  ga_population_append_receive( pop, src_node );

  return pop;
  }


/**********************************************************************
  ga_population_send()
  synopsis:	Send population structure (excluding actual entities)
 		to another processor.
		It should be noted that neither the userdata nor the
		function definitions will be sent.
		Some other less useful data is also not transfered.
  parameters:
  return:
  last updated: 24 May 2002
 **********************************************************************/

void ga_population_send( population *pop, int dest_node )
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  mpi_send(&(pop->stable_size), 1, MPI_TYPE_INT, dest_node, GA_TAG_POPSTABLESIZE);
  mpi_send(&(pop->crossover_ratio), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_POPCROSSOVER);
  mpi_send(&(pop->mutation_ratio), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_POPMUTATION);
  mpi_send(&(pop->migration_ratio), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_POPMIGRATION);

  return;
  }


/**********************************************************************
  ga_population_send_all()
  synopsis:	Send population structure (including all entities)
 		to another processor.
  parameters:
  return:
  last updated: 24 Jan 2002
 **********************************************************************/

void ga_population_send_all( population *pop, int dest_node )
  {

  /* Note that checks are performed in the two called functions. */

  ga_population_send(pop, dest_node);
  ga_population_send_every(pop, dest_node);

  return;
  }


#if 0
/**********************************************************************
  ga_marktosend_entity()
  synopsis:	Mark an entity to be sent to another subpopulation
		(i.e. jump to another processor).
  parameters:
  return:
  last updated: 22/09/00
 **********************************************************************/

void ga_marktosend_entity(int *send_mask)
  {
  }
#endif


#if 0
/**********************************************************************
  ga_multiproc_compare_entities()
  synopsis:	Synchronise processors and if appropriate transfer
		better solution to this processor.
		local will contain the optimum solution from local
		and localnew on all processors.
  parameters:
  return:
  last updated:	18/12/00
 **********************************************************************/

entity *ga_multiproc_compare_entities( population *pop, entity *localnew, entity *local )
  {
  double	global_max;		/* Maximum value across all nodes. */
  int		maxnode;		/* Node with maximum value. */
  int		*buffer=NULL;		/* Send/receive buffer. */
  int		*buffer_ptr=NULL;	/* Current position in end/receive buffer. */
  int		buffer_size;		/* Size of buffer. */
  int		j;			/* Loop over chromosomes. */
  entity	*tmpentity;		/* Entity ptr for swapping. */

  plog(LOG_FIXME, "Warning... untested code.");

  maxnode = mpi_find_global_max(MAX(localnew->fitness, local->fitness), &global_max);

  buffer_size = pop->num_chromosomes*pop->len_chromosomes;
  buffer_ptr = buffer = s_malloc(buffer_size*sizeof(int));

  if (maxnode == mpi_get_rank())
    {
    if (localnew->fitness > local->fitness)
      {
      tmpentity = local;
      local = localnew;
      localnew = tmpentity;
      }

    for (j=0; j<pop->num_chromosomes; j++)
      {
      memcpy(buffer_ptr, local->chromosome[j], pop->len_chromosomes*sizeof(int));
      buffer_ptr += pop->len_chromosomes;
      }

    mpi_distribute( buffer, buffer_size, MPI_TYPE_INT, maxnode, GA_TAG_BESTSYNC );
    }
  else
    {
    mpi_distribute( buffer, buffer_size, MPI_TYPE_INT, maxnode, GA_TAG_BESTSYNC );

    for (j=0; j<pop->num_chromosomes; j++)
      {
      memcpy(local->chromosome[j], buffer_ptr, pop->len_chromosomes*sizeof(int));
      buffer_ptr += pop->len_chromosomes;
      }

    pop->evaluate(pop, local);
    if (local->fitness != global_max)
      dief("Best scores don't match %f %f.", local->fitness, global_max);
    }

  s_free(buffer);

  return local;
  }


/**********************************************************************
  ga_sendrecv_entities()
  synopsis:	Make entities change subpopulations based on the
		previously set mask. (i.e. entities jump to
		another processor).
		Currently only sends the genetic data and rebuilds the
		structure.
		FIXME: Send structural data too.
		(This functionality should be provided by a user
		specified callback.)
  parameters:
  return:
  last updated: 22/09/00
 **********************************************************************/

boolean ga_sendrecv_entities( population *pop, int *send_mask, int send_count )
  {
  int		i, j;			/* Loop over all chromosomes in all entities. */
  int		next, prev;		/* Processor to send/receive entities with. */
  int		*buffer=NULL;		/* Send/receive buffer. */
  int		*buffer_ptr=NULL;	/* Current position in end/receive buffer. */
  int		recv_count;		/* Number of entities to receive. */
  int		recv_size, send_size=0;	/* Size of buffer. */
  int		index=0;		/* Index of entity to send. */
  entity	*immigrant;		/* New entity. */

  plog(LOG_FIXME, "Warning... untested code.");

/* Checks */
  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !send_mask ) die("Null pointer to int array.");

  next = mpi_get_next_rank();
  prev = mpi_get_prev_rank();

/* Pack chromosomes into buffer. */
  if (send_count > 0)
    {
    send_size = send_count*pop->num_chromosomes*pop->len_chromosomes;
    if ( !(buffer=s_malloc(send_size*sizeof(int))) )
      die("Unable to allocate memory.");

    buffer_ptr = buffer;

    for (i=0; i<send_count; i++)
      {
      while ( *send_mask == 0 )
        {	/* Skipping structure. */
        send_mask++;
        index++;
        }

      for (j=0; j<pop->num_chromosomes; j++)
        {
        memcpy(buffer_ptr,
               pop->entity_iarray[index]->chromosome[j],
               pop->len_chromosomes*sizeof(int));
        buffer_ptr += pop->len_chromosomes;
        }

      send_mask++;	/* Ready for next loop */
      index++;
      }
    }

/* Send data to next node. */
  plog(LOG_DEBUG, "Sending %d to node %d.", send_count, next);
  mpi_send( &send_count, 1, MPI_TYPE_INT, next, GA_TAG_MIGRATIONINFO );

  if (send_count > 0)
    {
    plog(LOG_DEBUG, "Sending %d ints to node %d.", send_size, next);
    mpi_send( buffer, send_size, MPI_TYPE_INT, next, GA_TAG_MIGRATIONDATA );
    }

/*
  helga_start_timer();
*/

/* Recieve data from previous node. */
  plog(LOG_DEBUG, "Recieving messages from node %d.", prev);

  mpi_receive( &recv_count, 1, MPI_TYPE_INT, prev, GA_TAG_MIGRATIONINFO );

  plog(LOG_DEBUG, "Will be receiving %d entities = %d ints (%Zd bytes).",
            recv_count,
            recv_count*pop->num_chromosomes*pop->len_chromosomes,
            recv_count*pop->num_chromosomes*pop->len_chromosomes*sizeof(int));

  if (recv_count > 0)
    {
    recv_size = recv_count*pop->num_chromosomes*pop->len_chromosomes;
    if ( !(buffer=s_realloc(buffer, recv_size*sizeof(int))) )
      die("Unable to reallocate memory.");

    buffer_ptr = buffer;

    mpi_receive( buffer, recv_size, MPI_TYPE_INT, prev, GA_TAG_MIGRATIONDATA );

    for (i=0; i<recv_count; i++)
      {
      immigrant = ga_get_free_entity(pop);
      for (j=0; j<pop->num_chromosomes; j++)
        {
        memcpy(buffer_ptr,
               immigrant->chromosome[j],
               pop->len_chromosomes*sizeof(int));
        buffer_ptr += pop->len_chromosomes;
        }
      pop->evaluate(pop, immigrant);

/*
      plog(LOG_VERBOSE, "Immigrant has fitness %f", immigrant->fitness);
*/
      }
    }

/* How much time did we waste? */
/*
  helga_check_timer();
*/

  if (buffer) s_free(buffer);

  return TRUE;
  }
#endif


/**********************************************************************
  Environmental operator function.
 **********************************************************************/

/**********************************************************************
  ga_optimise_entity()
  synopsis:	Optimise the entity's structure through local
		searching in the gene space.
		Should be default choice for "adaptation" function.
		The original entity will be left untouched.
  parameters:
  return:
  last updated: 24 Oct 2002
 **********************************************************************/

entity *ga_optimise_entity(population *pop, entity *unopt)
  {
  entity	*optimised;

  /* Checks */
  if ( !pop ) die("Null pointer to population structure passed.");
  if ( !unopt ) die("Null pointer to entity structure passed.");

  plog(LOG_FIXME,
       "Code incomplete, using 25 iterations of the RAHC algorithm for now.");

  optimised = ga_entity_clone(pop, unopt);

/* FIXME: hard-coded value. */
  ga_random_ascent_hillclimbing( pop, optimised, 25 );

  plog(LOG_DEBUG,
       "Entity optimised from %f to %f.",
       unopt->fitness, optimised->fitness);

  return optimised;
  }


/**********************************************************************
  GA functions.
 **********************************************************************/

/**********************************************************************
  ga_population_set_parameters()
  synopsis:	Sets the GA parameters for a population.
  parameters:
  return:
  last updated:	10 Jun 2002
 **********************************************************************/

void ga_population_set_parameters(	population		*pop,
					const ga_scheme_type	scheme,
					const ga_elitism_type	elitism,
					const double		crossover,
					const double		mutation,
					const double		migration)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE,
        "Population's parameters: scheme = %d elitism = %d crossover = %f mutation = %f migration = %f",
        (int) scheme, (int) elitism,
        crossover, mutation, migration );

  pop->scheme = scheme;
  pop->elitism = elitism;
  pop->crossover_ratio = crossover;
  pop->mutation_ratio = mutation;
  pop->migration_ratio = migration;

  return;
  }


/**********************************************************************
  ga_population_set_scheme()
  synopsis:	Sets the evolutionary class for a population.
  parameters:
  return:
  last updated:	10 Jun 2002
 **********************************************************************/

void ga_population_set_scheme(	population		*pop,
				const ga_scheme_type	scheme)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE, "Population's evolutionary class = %d", (int) scheme);

  pop->scheme = scheme;

  return;
  }


/**********************************************************************
  ga_population_set_elitism()
  synopsis:	Sets the elitism mode for a population.
  parameters:
  return:
  last updated:	10 Jun 2002
 **********************************************************************/

void ga_population_set_elitism(	population		*pop,
				const ga_elitism_type	elitism)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE, "Population's elitism mode = %d", (int) elitism);

  pop->elitism = elitism;

  return;
  }


/**********************************************************************
  ga_population_set_mutation()
  synopsis:	Sets the mutation rate for a population.
  parameters:
  return:
  last updated:	10 Jun 2002
 **********************************************************************/

void ga_population_set_mutation(	population	*pop,
					const double	mutation)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE, "Population's mutation rate = %f", mutation);

  pop->mutation_ratio = mutation;

  return;
  }


/**********************************************************************
  ga_population_set_migration()
  synopsis:	Sets the migration rate for a population.
  parameters:
  return:
  last updated:	10 Jun 2002
 **********************************************************************/

void ga_population_set_migration(	population	*pop,
					const double	migration)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE, "Population's migration rate = %f", migration);

  pop->migration_ratio = migration;

  return;
  }


/**********************************************************************
  ga_population_set_crossover()
  synopsis:	Sets the crossover rate a population.
  parameters:
  return:
  last updated:	10 Jun 2002
 **********************************************************************/

void ga_population_set_crossover(	population	*pop,
					const double	crossover)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE, "Population's crossover rate = %f", crossover);

  pop->crossover_ratio = crossover;

  return;
  }


/**********************************************************************
  ga_transcend()
  synopsis:	Return a population structure to user for analysis or
		whatever.  But remove it from the population table.
		(Like ga_extinction, except doesn't purge memory.)
  parameters:   unsigned int	population id
  return:       population *	population pointer (or NULL)
  last updated:	15 Aug 2002
 **********************************************************************/

population *ga_transcend(unsigned int id)
  {
  population	*pop=NULL;	/* Transcending population. */

  plog(LOG_VERBOSE, "This population has achieved transcendance!");

  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    pop = (population *) table_remove_index(pop_table, id);
    if (table_count_items(pop_table) < 1)
      {
      table_destroy(pop_table);
      pop_table=NULL;
      }
    }
  THREAD_UNLOCK(pop_table_lock);

  return pop;
  }


/**********************************************************************
  ga_resurect()
  synopsis:	Restores a population structure into the population
		table from an external source.
  parameters:	population *	population pointer
  return:       unsigned int	population id (or -1)
  last updated:	15 Aug 2002
 **********************************************************************/

unsigned int ga_resurect(population *pop)
  {
  unsigned int	id=TABLE_ERROR_INDEX;	/* Internal population id. */

  if ( !pop ) die("Null pointer to population structure passed.");

  plog(LOG_VERBOSE, "The population has been restored!");

  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    id = table_add(pop_table, pop);
    }
  THREAD_UNLOCK(pop_table_lock);

  return id;
  }


/**********************************************************************
  ga_extinction()
  synopsis:	Purge all memory used by a population, also remove
		it from the population table.
  parameters:
  return:
  last updated:	15 Aug 2002
 **********************************************************************/

boolean ga_extinction(population *extinct)
  {
  unsigned int	id = TABLE_ERROR_INDEX;	/* Internal index for this extinct population. */

  if ( !extinct ) die("Null pointer to population structure passed.");

  plog(LOG_VERBOSE, "This population is becoming extinct!");

/*
 * Remove this population from the population table.
 */
  THREAD_LOCK(pop_table_lock);
  if (pop_table)
    {
    id = table_remove_data(pop_table, extinct);
    if (table_count_items(pop_table) < 1)
      {
      table_destroy(pop_table);
      pop_table=NULL;
      }
    }
  THREAD_UNLOCK(pop_table_lock);

/*
 * Error check.
 */
  if (id == TABLE_ERROR_INDEX)
    die("Unable to find population structure in table.");

/*
 * Any user-data?
 */
  if (extinct->data)
    plog(LOG_WARNING, "User data field is not empty. (Potential memory leak)");

/*
 * Dereference/free everyting.
 */
  if (!ga_genocide(extinct, 0))
    {
    plog(LOG_NORMAL, "This population is already extinct!");
    }
  else
    {
    s_free(extinct->entity_array);
    s_free(extinct->entity_iarray);
    mem_chunk_destroy(extinct->entity_chunk);
    if (extinct->tabu_params) s_free(extinct->tabu_params);
    if (extinct->sa_params) s_free(extinct->sa_params);
    s_free(extinct);
    }

  return TRUE;
  }


/**********************************************************************
  ga_genocide()
  synopsis:	Kill entities to reduce population size down to
		specified value.
  parameters:
  return:
  last updated:	22 Aug 2002
 **********************************************************************/

boolean ga_genocide(population *pop, int target_size)
  {
  if ( !pop ) return FALSE;

  plog(LOG_VERBOSE,
            "The population is being culled from %d to %d individuals!",
            pop->size, target_size);

/*
 * Dereference the structures relating to the least
 * fit population members until the desired population size in reached. 
 */
  while (pop->size>target_size)
    {
/*printf("Dereferencing entity with rank %d\n", pop->size-1);*/
    ga_entity_dereference_by_rank(pop, pop->size-1);
    }

  return TRUE;
  }


/**********************************************************************
  ga_entity_get_fitness()
  synopsis:	Gets an entity's fitness.
  parameters:
  return:
  last updated: 23 May 2002
 **********************************************************************/

double ga_entity_get_fitness(entity *e)
  {

  return e?e->fitness:GA_MIN_FITNESS;
  }


/**********************************************************************
  ga_entity_set_fitness()
  synopsis:	Gets an entity's fitness.
  parameters:
  return:
  last updated: 23 May 2002
 **********************************************************************/

boolean ga_entity_set_fitness(entity *e, double fitness)
  {
  if ( !e ) return FALSE;

  e->fitness=fitness;

  return TRUE;
  }


/**********************************************************************
  ga_population_get_stablesize()
  synopsis:	Gets a population's stable size.
  parameters:
  return:
  last updated: 23 May 2002
 **********************************************************************/

int ga_population_get_stablesize(population *pop)
  {

  return pop?pop->stable_size:0;
  }


/**********************************************************************
  ga_population_get_size()
  synopsis:	Gets a population's current size.
  parameters:
  return:
  last updated: 23 May 2002
 **********************************************************************/

int ga_population_get_size(population *pop)
  {

  return pop?pop->size:0;
  }


/**********************************************************************
  ga_population_get_maxsize()
  synopsis:	Gets a population's maximum size. (I don't know why
		anyone would need this function, but it is here for
		completeness.)
  parameters:
  return:
  last updated: 23 May 2002
 **********************************************************************/

int ga_population_get_maxsize(population *pop)
  {

  return pop?pop->max_size:0;
  }


/**********************************************************************
  ga_population_set_stablesize()
  synopsis:	Gets a population's stable size.
  parameters:
  return:
  last updated: 23 May 2002
 **********************************************************************/

boolean ga_population_set_stablesize(population *pop, int stable_size)
  {
  if ( !pop ) return FALSE;

  pop->stable_size = stable_size;

  return TRUE;
  }


/**********************************************************************
  ga_population_set_data()
  synopsis:	Sets the population's user data.
  parameters:
  return:
  last updated: 08 Nov 2002
 **********************************************************************/

boolean ga_population_set_data(population *pop, vpointer data)
  {
  if ( !pop ) return FALSE;

  pop->data = data;

  return TRUE;
  }


/**********************************************************************
  ga_population_get_data()
  synopsis:	Gets the population's user data.
  parameters:
  return:
  last updated: 08 Nov 2002
 **********************************************************************/

vpointer ga_population_get_data(population *pop)
  {
  if ( !pop ) return NULL;

  return pop->data;
  }


/**********************************************************************
  ga_entity_set_data()
  synopsis:	Sets the entity's user data.
  parameters:
  return:
  last updated: 08 Nov 2002
 **********************************************************************/

boolean ga_entity_set_data(population *pop, entity *e, SLList *data)
  {
  SLList	*this;		/* Current list element. */

  if ( !pop ) return FALSE;
  if ( !e ) return FALSE;

  if (e->data)
    {
    if (pop->data_destructor)
      {
      this = data;
      while (this!=NULL)
        {
        pop->data_destructor(slink_data(this));
        this = slink_next(this);
        }
      }
    slink_free_all(e->data);
    }

  e->data = data;

  return TRUE;
  }


/**********************************************************************
  ga_entity_get_data()
  synopsis:	Gets the entity's user data.
  parameters:
  return:
  last updated: 08 Nov 2002
 **********************************************************************/

SLList *ga_entity_get_data(population *pop, entity *e)
  {

  if ( !pop ) return NULL;
  if ( !e ) return NULL;

  return e->data;
  }


/**********************************************************************
  ga_population_get_generation()
  synopsis:	Gets the current generation number.  Intended for use
		within fitness evaluation callbacks only.
  parameters:
  return:
  last updated: 18 Mar 2003
 **********************************************************************/

int ga_population_get_generation(population *pop)
  {

  if ( !pop ) return 0;

  return pop->generation;
  }


