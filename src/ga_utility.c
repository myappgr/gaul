/**********************************************************************
  ga_utility.c
 **********************************************************************

  ga_utility - High-level genetic algorithm routines.
  Copyright �2000-2001, Stewart Adcock <stewart@bellatrix.pcl.ox.ac.uk>

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

  Synopsis:     High-level GA functions and convenience functions.

  To do:	Population/entity iterator functions.
		On-line and off-line performance summaries.
		The ga_genesis_XXX() functions are bloated junk which duplicate loads of code and are a complete mess.  These need sorting out!

 **********************************************************************/

#include "ga_utility.h"

/**********************************************************************
  ga_diagnostics()
  synopsis:	Diagnostics.
  parameters:
  return:	none
  last updated:	13/06/01
 **********************************************************************/

void ga_diagnostics(void)
  {
  int	num_pops;	/* Number of populations defined, or -1 for no table. */

  printf("=== GA utility library =======================================\n");
  printf("Version:                   %s\n", VERSION_STRING);
  printf("Build date:                %s\n", BUILD_DATE_STRING);
  printf("--------------------------------------------------------------\n");
  printf("GA_DEBUG:                  %d\n", GA_DEBUG);
  printf("GA_BOLTZMANN_FACTOR:       %f\n", GA_BOLTZMANN_FACTOR);
  printf("GA_MIN_FITNESS:            %f\n", GA_MIN_FITNESS);
  printf("GA_MULTI_BIT_CHANCE:       %f\n", GA_MULTI_BIT_CHANCE);
  printf("GA_ELITISM_MULTIPLIER:     %f\n", GA_ELITISM_MULTIPLIER);
  printf("GA_ELITISM_CONSTANT:       %f\n", GA_ELITISM_CONSTANT);
  printf("BYTEBITS:                  %d\n", BYTEBITS);
  printf("--------------------------------------------------------------\n");
  printf("structure                  sizeof\n");
  printf("population                 %Zd\n", sizeof(population));
  printf("entity                     %Zd\n", sizeof(entity));
  printf("byte                       %Zd\n", sizeof(byte));
  printf("--------------------------------------------------------------\n");
  num_pops = ga_get_num_populations();
  if (num_pops==-1)
    {
    printf("Population table:          undefined\n");
    }
  else
    {
    printf("Population table:          defined\n");
    printf("Size:                      %d\n", num_pops);
    }
  printf("--------------------------------------------------------------\n");

  return;
  }


/**********************************************************************
  ga_genesis()
  synopsis:	High-level function to create a new population and
		perform the basic setup (i.e. initial seeding) required
		for further optimisation and manipulation.
		Assumes the use of integer chromosomes is desired.
		This currently only exists for compatibility with
		older versions of GAUL.
		Integer-valued chromsomes.
  parameters:
  return:	population, or NULL on failure.
  last updated:	13/06/01
 **********************************************************************/

population *ga_genesis(	const int		population_size,
			const int		num_chromo,
			const int		len_chromo,
			GAgeneration_hook	generation_hook,
			GAiteration_hook	iteration_hook,
			GAdata_destructor	data_destructor,
			GAdata_ref_incrementor	data_ref_incrementor,
			GAevaluate		evaluate,
			GAseed			seed,
			GAadapt			adapt,
			GAselect_one		select_one,
			GAselect_two		select_two,
			GAmutate		mutate,
			GAcrossover		crossover,
			GAreplace		replace )
  {
  population	*pop;	/* The new population structure. */

  plog(LOG_VERBOSE, "Genesis is beginning!");
  plog(LOG_FIXME, "There are hard coded values in ga_genesis().");

/*
 * Allocate and initialise a new population.
 * This call also sets this as the active population.
 *
 * FIXME:
 * The hard-coded value below "4(N+2)" should be determined based on the
 * actual mutation and crossover rates to be used.
 */
  if ( !(pop = ga_population_new( 4*(population_size+2),
                              population_size,
                              num_chromo,
                              len_chromo )) ) return NULL;

/*
 * Define some callback functions.
 */
  pop->generation_hook = generation_hook;
  pop->iteration_hook = iteration_hook;

  pop->data_destructor = data_destructor;
  pop->data_ref_incrementor = data_ref_incrementor;

  pop->chromosome_constructor = ga_chromosome_integer_allocate;
  pop->chromosome_destructor = ga_chromosome_integer_deallocate;
  pop->chromosome_replicate = ga_chromosome_integer_replicate;
  pop->chromosome_to_bytes = ga_chromosome_integer_to_bytes;
  pop->chromosome_from_bytes = ga_chromosome_integer_from_bytes;
  pop->chromosome_to_string = ga_chromosome_integer_to_staticstring;

  pop->evaluate = evaluate;
  pop->seed = seed;
  pop->adapt = adapt;
  pop->select_one = select_one;
  pop->select_two = select_two;
  pop->mutate = mutate;
  pop->crossover = crossover;
  pop->replace = replace;

/*
 * Seed the population.
 */
  if (!seed)
    {
    plog(LOG_VERBOSE, "Entity seed function not defined.  Genesis can not occur.  Continuing anyway.");
    }
  else
    {
    ga_population_seed(pop);
    plog(LOG_VERBOSE, "Genesis has occured!");
    }

  return pop;
  }


/**********************************************************************
  ga_genesis_char()
  synopsis:	High-level function to create a new population and
		perform the basic setup (i.e. initial seeding) required
		for further optimisation and manipulation.
		Assumes the use of integer chromosomes is desired.
		This currently only exists for compatibility with
		older versions of GAUL.
		Character-valued chromosomes.
  parameters:
  return:	population, or NULL on failure.
  last updated:	16/06/01
 **********************************************************************/

population *ga_genesis_char(	const int		population_size,
			const int		num_chromo,
			const int		len_chromo,
			GAgeneration_hook	generation_hook,
			GAiteration_hook	iteration_hook,
			GAdata_destructor	data_destructor,
			GAdata_ref_incrementor	data_ref_incrementor,
			GAevaluate		evaluate,
			GAseed			seed,
			GAadapt			adapt,
			GAselect_one		select_one,
			GAselect_two		select_two,
			GAmutate		mutate,
			GAcrossover		crossover,
			GAreplace		replace )
  {
  population	*pop;	/* The new population structure. */

  plog(LOG_VERBOSE, "Genesis is beginning!");
  plog(LOG_FIXME, "There are hard coded values in ga_genesis().");

/*
 * Allocate and initialise a new population.
 * This call also sets this as the active population.
 *
 * FIXME:
 * The hard-coded value below "4(N+2)" should be determined based on the
 * actual mutation and crossover rates to be used.
 */
  if ( !(pop = ga_population_new( 4*(population_size+2),
                              population_size,
                              num_chromo,
                              len_chromo )) ) return NULL;

/*
 * Define some callback functions.
 */
  pop->generation_hook = generation_hook;
  pop->iteration_hook = iteration_hook;

  pop->data_destructor = data_destructor;
  pop->data_ref_incrementor = data_ref_incrementor;

  pop->chromosome_constructor = ga_chromosome_char_allocate;
  pop->chromosome_destructor = ga_chromosome_char_deallocate;
  pop->chromosome_replicate = ga_chromosome_char_replicate;
  pop->chromosome_to_bytes = ga_chromosome_char_to_bytes;
  pop->chromosome_from_bytes = ga_chromosome_char_from_bytes;
  pop->chromosome_to_string = ga_chromosome_char_to_staticstring;

  pop->evaluate = evaluate;
  pop->seed = seed;
  pop->adapt = adapt;
  pop->select_one = select_one;
  pop->select_two = select_two;
  pop->mutate = mutate;
  pop->crossover = crossover;
  pop->replace = replace;

/*
 * Seed the population.
 */
  if (!seed)
    {
    plog(LOG_VERBOSE, "Entity seed function not defined.  Genesis can not occur.  Continuing anyway.");
    }
  else
    {
    ga_population_seed(pop);
    plog(LOG_VERBOSE, "Genesis has occured!");
    }

  return pop;
  }


/**********************************************************************
  ga_genesis_boolean()
  synopsis:	High-level function to create a new population and
		perform the basic setup (i.e. initial seeding) required
		for further optimisation and manipulation.
		Assumes the use of integer chromosomes is desired.
		This currently only exists for compatibility with
		older versions of GAUL.
		Boolean-valued chromosomes.
  parameters:
  return:	population, or NULL on failure.
  last updated:	16/06/01
 **********************************************************************/

population *ga_genesis_boolean(	const int		population_size,
			const int		num_chromo,
			const int		len_chromo,
			GAgeneration_hook	generation_hook,
			GAiteration_hook	iteration_hook,
			GAdata_destructor	data_destructor,
			GAdata_ref_incrementor	data_ref_incrementor,
			GAevaluate		evaluate,
			GAseed			seed,
			GAadapt			adapt,
			GAselect_one		select_one,
			GAselect_two		select_two,
			GAmutate		mutate,
			GAcrossover		crossover,
			GAreplace		replace )
  {
  population	*pop;	/* The new population structure. */

  plog(LOG_VERBOSE, "Genesis is beginning!");
  plog(LOG_FIXME, "There are hard coded values in ga_genesis().");

/*
 * Allocate and initialise a new population.
 * This call also sets this as the active population.
 *
 * FIXME:
 * The hard-coded value below "4(N+2)" should be determined based on the
 * actual mutation and crossover rates to be used.
 */
  if ( !(pop = ga_population_new( 4*(population_size+2),
                              population_size,
                              num_chromo,
                              len_chromo )) ) return NULL;

/*
 * Define some callback functions.
 */
  pop->generation_hook = generation_hook;
  pop->iteration_hook = iteration_hook;

  pop->data_destructor = data_destructor;
  pop->data_ref_incrementor = data_ref_incrementor;

  pop->chromosome_constructor = ga_chromosome_boolean_allocate;
  pop->chromosome_destructor = ga_chromosome_boolean_deallocate;
  pop->chromosome_replicate = ga_chromosome_boolean_replicate;
  pop->chromosome_to_bytes = ga_chromosome_boolean_to_bytes;
  pop->chromosome_from_bytes = ga_chromosome_boolean_from_bytes;
  pop->chromosome_to_string = ga_chromosome_boolean_to_staticstring;

  pop->evaluate = evaluate;
  pop->seed = seed;
  pop->adapt = adapt;
  pop->select_one = select_one;
  pop->select_two = select_two;
  pop->mutate = mutate;
  pop->crossover = crossover;
  pop->replace = replace;

/*
 * Seed the population.
 */
  if (!seed)
    {
    plog(LOG_VERBOSE, "Entity seed function not defined.  Genesis can not occur.  Continuing anyway.");
    }
  else
    {
    ga_population_seed(pop);
    plog(LOG_VERBOSE, "Genesis has occured!");
    }

  return pop;
  }


/**********************************************************************
  ga_genesis_double()
  synopsis:	High-level function to create a new population and
		perform the basic setup (i.e. initial seeding) required
		for further optimisation and manipulation.
		Assumes the use of integer chromosomes is desired.
		This currently only exists for compatibility with
		older versions of GAUL.
		Double precision real-valued chromosomes.
  parameters:
  return:	population, or NULL on failure.
  last updated:	16/06/01
 **********************************************************************/

population *ga_genesis_double(	const int		population_size,
			const int		num_chromo,
			const int		len_chromo,
			GAgeneration_hook	generation_hook,
			GAiteration_hook	iteration_hook,
			GAdata_destructor	data_destructor,
			GAdata_ref_incrementor	data_ref_incrementor,
			GAevaluate		evaluate,
			GAseed			seed,
			GAadapt			adapt,
			GAselect_one		select_one,
			GAselect_two		select_two,
			GAmutate		mutate,
			GAcrossover		crossover,
			GAreplace		replace )
  {
  population	*pop;	/* The new population structure. */

  plog(LOG_VERBOSE, "Genesis is beginning!");
  plog(LOG_FIXME, "There are hard coded values in ga_genesis().");

/*
 * Allocate and initialise a new population.
 * This call also sets this as the active population.
 *
 * FIXME:
 * The hard-coded value below "4(N+2)" should be determined based on the
 * actual mutation and crossover rates to be used.
 */
  if ( !(pop = ga_population_new( 4*(population_size+2),
                              population_size,
                              num_chromo,
                              len_chromo )) ) return NULL;

/*
 * Define some callback functions.
 */
  pop->generation_hook = generation_hook;
  pop->iteration_hook = iteration_hook;

  pop->data_destructor = data_destructor;
  pop->data_ref_incrementor = data_ref_incrementor;

  pop->chromosome_constructor = ga_chromosome_double_allocate;
  pop->chromosome_destructor = ga_chromosome_double_deallocate;
  pop->chromosome_replicate = ga_chromosome_double_replicate;
  pop->chromosome_to_bytes = ga_chromosome_double_to_bytes;
  pop->chromosome_from_bytes = ga_chromosome_double_from_bytes;
  pop->chromosome_to_string = ga_chromosome_double_to_staticstring;

  pop->evaluate = evaluate;
  pop->seed = seed;
  pop->adapt = adapt;
  pop->select_one = select_one;
  pop->select_two = select_two;
  pop->mutate = mutate;
  pop->crossover = crossover;
  pop->replace = replace;

/*
 * Seed the population.
 */
  if (!seed)
    {
    plog(LOG_VERBOSE, "Entity seed function not defined.  Genesis can not occur.  Continuing anyway.");
    }
  else
    {
    ga_population_seed(pop);
    plog(LOG_VERBOSE, "Genesis has occured!");
    }

  return pop;
  }


/**********************************************************************
  ga_genesis_bitstring()
  synopsis:	High-level function to create a new population and
		perform the basic setup (i.e. initial seeding) required
		for further optimisation and manipulation.
		Assumes the use of integer chromosomes is desired.
		This currently only exists for compatibility with
		older versions of GAUL.
		Bitstring-valued chromosomes.
  parameters:
  return:	population, or NULL on failure.
  last updated:	30/06/01
 **********************************************************************/

population *ga_genesis_bitstring(	const int		population_size,
			const int		num_chromo,
			const int		len_chromo,
			GAgeneration_hook	generation_hook,
			GAiteration_hook	iteration_hook,
			GAdata_destructor	data_destructor,
			GAdata_ref_incrementor	data_ref_incrementor,
			GAevaluate		evaluate,
			GAseed			seed,
			GAadapt			adapt,
			GAselect_one		select_one,
			GAselect_two		select_two,
			GAmutate		mutate,
			GAcrossover		crossover,
			GAreplace		replace )
  {
  population	*pop;	/* The new population structure. */

  plog(LOG_VERBOSE, "Genesis is beginning!");
  plog(LOG_FIXME, "There are hard coded values in ga_genesis().");

/*
 * Allocate and initialise a new population.
 * This call also sets this as the active population.
 *
 * FIXME:
 * The hard-coded value below "4(N+2)" should be determined based on the
 * actual mutation and crossover rates to be used.
 */
  if ( !(pop = ga_population_new( 4*(population_size+2),
                              population_size,
                              num_chromo,
                              len_chromo )) ) return NULL;

/*
 * Define some callback functions.
 */
  pop->generation_hook = generation_hook;
  pop->iteration_hook = iteration_hook;

  pop->data_destructor = data_destructor;
  pop->data_ref_incrementor = data_ref_incrementor;

  pop->chromosome_constructor = ga_chromosome_bitstring_allocate;
  pop->chromosome_destructor = ga_chromosome_bitstring_deallocate;
  pop->chromosome_replicate = ga_chromosome_bitstring_replicate;
  pop->chromosome_to_bytes = ga_chromosome_bitstring_to_bytes;
  pop->chromosome_from_bytes = ga_chromosome_bitstring_from_bytes;
  pop->chromosome_to_string = ga_chromosome_bitstring_to_staticstring;

  pop->evaluate = evaluate;
  pop->seed = seed;
  pop->adapt = adapt;
  pop->select_one = select_one;
  pop->select_two = select_two;
  pop->mutate = mutate;
  pop->crossover = crossover;
  pop->replace = replace;

/*
 * Seed the population.
 */
  if (!seed)
    {
    plog(LOG_VERBOSE, "Entity seed function not defined.  Genesis can not occur.  Continuing anyway.");
    }
  else
    {
    ga_population_seed(pop);
    plog(LOG_VERBOSE, "Genesis has occured!");
    }

  return pop;
  }


/**********************************************************************
  ga_allele_search()
  synopsis:	Perform complete systematic search on a given allele
		in a given entity.  If initial solution is NULL, then
		a random solution is generated (but use of that feature
		is unlikely to be useful!).
		The original entity will not be munged.
                NOTE: max_val is the maximum value _+_ 1!
		WARNING: Now only works for integer array chromosomes!
		FIXME: Need to make chromosome datatype agnostic.
  parameters:
  return:	Best solution found.
  last updated:	24/03/01
 **********************************************************************/

entity *ga_allele_search(	population	*pop,
				const int	chromosomeid,
				const int	point,
				const int 	min_val,
				const int 	max_val,
				entity		*initial )
  {
  int		val;			/* Current value for point. */
  entity	*current, *best;	/* The solutions. */

/* Checks. */
/* FIXME: More checks needed. */
  if ( !pop ) die("Null pointer to population structure passed.");

  current = ga_get_free_entity(pop);	/* The 'working' solution. */
  best = ga_get_free_entity(pop);	/* The best solution so far. */

  plog(LOG_FIXME, "Systematic allele search algorithm is not parallelised.");

/* Do we need to generate a random solution? */
  if (!initial)
    {
    plog(LOG_VERBOSE, "Will perform systematic allele search with random starting solution.");

    pop->seed(pop, best);
    }
  else
    {
    plog(LOG_VERBOSE, "Will perform systematic allele search.");

    ga_entity_copy(pop, best, initial);
    }

/*
 * Copy best solution over current solution.
 */
  ga_entity_copy(pop, current, best);
  best->fitness=GA_MIN_FITNESS;

/*
 * Loop over the range of legal values.
 */
  for (val = min_val; val < max_val; val++)
    {
    ((int *)current->chromosome[chromosomeid])[point] = val;
    ga_entity_clear_data(pop, current, chromosomeid);

    pop->evaluate(pop, current);

/*
 * Should we keep this solution?
 */
    if ( best->fitness < current->fitness )
      { /* Copy this solution best solution. */
      ga_entity_blank(pop, best);
      ga_entity_copy(pop, best, current);
      }
    else
      { /* Copy best solution over current solution. */
      ga_entity_blank(pop, current);
      ga_entity_copy(pop, current, best);
      }

    }

  plog(LOG_VERBOSE,
            "After complete search the best solution has fitness score of %f",
            best->fitness );

/*
 * Current no longer needed.  It is upto the caller to dereference the
 * optimum solution found.
 */
  ga_entity_dereference(pop, current);

  return best;
  }


/**********************************************************************
  ga_population_dump()
  synopsis:	Dump some statistics about a population.
  parameters:	population	*pop
  return:	none
  last updated:	16/06/01
 **********************************************************************/

void ga_population_dump(population	*pop)
  {
  printf("Population id %d\n", ga_get_population_id(pop));
  printf("Max size %d Stable size %d Current size %d\n", pop->max_size, pop->stable_size, pop->size);
  printf("Crossover %f Mutation %f Migration %f\n", pop->crossover_ratio, pop->mutation_ratio, pop->migration_ratio);
  printf("Chromosome length %d count %d\n", pop->len_chromosomes, pop->num_chromosomes);
  printf("Best fitness %f\n", pop->entity_iarray[0]->fitness);

  return;
  }


/**********************************************************************
  ga_entity_dump()
  synopsis:	Dump some statistics about a entity.
  parameters:	entity	*john
  return:	none
  last updated:	16/06/01
 **********************************************************************/

void ga_entity_dump(population *pop, entity *john)
  {
  printf("Entity id %d rank %d\n", ga_get_entity_id(pop, john), ga_get_entity_rank(pop, john));
  printf("Fitness %f\n", john->fitness);
  printf("data <%s> data0 <%s> chromo <%s> chromo0 <%s>\n", john->data?"defined":"undefined", john->data&&((SLList *)john->data)->data?"defined":"undefined", john->chromosome?"defined":"undefined", john->chromosome&&john->chromosome[0]?"defined":"undefined");

  return;
  }
