<?php
/**
 * 
 * @file taxonomy_easer.inc
 * In development.
 * This file provides set of functions to easy work with taxonomy.
 * Use <code>
 * module_load_include('inc', 'ai_home_page', 'includes/taxonomy_easer.inc');
 * </code> to load this .inc 
 * Project pages http://itech4web.com/taxonomy_easer,  http://drupal.org/sandbox/redhead/1800096
 */

/**
 * This function return all childrens for selected terms
 * @params object $term
 * @return array of objects of all children terms
 */
function taxonomy_easer_taxonomy_get_children_all (object $term) {
  $terms = array();
  if ($children = taxonomy_get_children($term->tid)) {  
    $marked_children = _taxonomy_easer_get_children_all_mark_child($children);
    $terms = _taxonomy_easer_get_children_all_extract_child($marked_children);
  }
  return $terms;
}

/**
 * Helper for taxonomy_easer_taxonomy_get_children_all()
 *
 *@param
 *@return
 */
function _taxonomy_easer_get_children_all_mark_child(array $terms)  {
  foreach($terms as $k => $term){
    if($children = taxonomy_get_children(($term->tid))){
      $terms['has_children:' . $k ] = _taxonomy_easer_get_children_all_mark_child($children);
    }
  }
  return $terms;
} 

/**
 * Helper for taxonomy_easer_taxonomy_get_children_all()
 *
 *@param
 *@return
 */
function _taxonomy_easer_get_children_all_extract_child(array $terms) {
  $children = &drupal_static(__function__ . '_children', array());
  
  foreach($terms as $key => $child){
    list($has_children) = explode(':', $key);
    if($has_children == 'has_children'){
      _taxonomy_easer_get_children_all_extract_child($child); 
    }else{
      $children[] = $child;
    }
  }
  return $children;
} 


/**
 * 
 * @return array multilevel array grouped by top level 
 *  term and its children.
 * @todo need review
 * 
 */
function taxonomy_easer_get_children_all_tids($vocabulary_machine_name, $add_dashes = FALSE) {
  $vocabulary = taxonomy_vocabulary_machine_name_load($vocabulary_machine_name);
  $terms = taxonomy_get_tree($vocabulary->vid);
  
  $tree = array();

  foreach ($terms as $term) {
    $tree[$term->tid] = $term;
  }
  
  foreach ($tree as $item) {
    if (isset($tree[$item->parents[0]])) {
      $tree[$item->parents[0]]->children[$item->tid] = $item;
    }
  }
  
  foreach ($tree as $item) {
    if ($tree[$item->tid]->parents[0] !=0 ) {
      unset($tree[$item->tid]);
    }  
  }
  
  $new_tree = array();
  foreach ($tree as $item) {
    if (drupal_strtolower($item->name) != 'home') {
      $new_tree[$item->tid]['name'] = $item->name;
      $new_tree[$item->tid]['tid'] = $item->tid;
      $new_tree[$item->tid]['children'] = _taxonomy_easer_get_nested_child($item, $add_dashes);
    }
  }
  
  return $new_tree;
}

/**
 * Build nested tree from term.
 * Add "-" prefix depend on term deepth
 * @param array $term with "children" sub array
 * @return array keyed by child tid
 */
function _taxonomy_easer_get_nested_child($term, $add_dashes)  {
  $children = &drupal_static(__function__ . '_children', array());
  
  if ($term->parents[0] == 0) {
    drupal_static_reset(__function__ . '_children');
  }
  if (isset($term->children)) {
    foreach ($term->children as $child) {
      $children[$child->tid] = $add_dashes ? str_repeat('-', $child->depth) . $child->name : $child->name;
      if (isset($child->children)) {
        _taxonomy_easer_get_nested_child($child, $add_dashes);        
      }
    }
    return $children;
  }  
}

/**
 *
 *
 *@param array $filter = array(
 *  'column' => 'name',
 *  'value' => 'val', // string or array
 *  'operator' => = '=' // in, like                   
 *  );
 *@return
 */
function taxonomy_easer_get_filtered_tree($vid, $parent = 0, $max_depth = NULL, $load_entities = FALSE, $filter = array()) {
  $children = &drupal_static(__FUNCTION__, array());
  $parents = &drupal_static(__FUNCTION__ . ':parents', array());
  $terms = &drupal_static(__FUNCTION__ . ':terms', array());

  // We cache trees, so it's not CPU-intensive to call taxonomy_get_tree() on a
  // term and its children, too.
  if (!isset($children[$vid])) {
    $children[$vid] = array();
    $parents[$vid] = array();
    $terms[$vid] = array();

    $query = db_select('taxonomy_term_data', 't');
    $query->join('taxonomy_term_hierarchy', 'h', 'h.tid = t.tid');
     $query
      ->addTag('translatable')
      ->addTag('term_access')
      ->fields('t')
      ->fields('h', array('parent'))
      ->condition('t.vid', $vid);
       if ($filter) {
         //dpm($filter);
         $query->condition("t.{$filter['column']}", $filter['value'], $filter['operator']);
       }
      $query->orderBy('t.weight');
      $query->orderBy('t.name');
    $result = $query->execute();

    foreach ($result as $term) {
      $children[$vid][$term->parent][] = $term->tid;
      $parents[$vid][$term->tid][] = $term->parent;
      $terms[$vid][$term->tid] = $term;
    }
  }

  // Load full entities, if necessary. The entity controller statically
  // caches the results.
  if ($load_entities) {
    $term_entities = taxonomy_term_load_multiple(array_keys($terms[$vid]));
  }

  $max_depth = (!isset($max_depth)) ? count($children[$vid]) : $max_depth;
  $tree = array();

  // Keeps track of the parents we have to process, the last entry is used
  // for the next processing step.
  $process_parents = array();
  $process_parents[] = $parent;

  // Loops over the parent terms and adds its children to the tree array.
  // Uses a loop instead of a recursion, because it's more efficient.
  while (count($process_parents)) {
    $parent = array_pop($process_parents);
    // The number of parents determines the current depth.
    $depth = count($process_parents);
    if ($max_depth > $depth && !empty($children[$vid][$parent])) {
      $has_children = FALSE;
      $child = current($children[$vid][$parent]);
      do {
        if (empty($child)) {
          break;
        }
        $term = $load_entities ? $term_entities[$child] : $terms[$vid][$child];
        if (isset($parents[$vid][$term->tid])) {
          // Clone the term so that the depth attribute remains correct
          // in the event of multiple parents.
          $term = clone $term;
        }
        $term->depth = $depth;
        unset($term->parent);
        $term->parents = $parents[$vid][$term->tid];
        $tree[] = $term;
        if (!empty($children[$vid][$term->tid])) {
          $has_children = TRUE;

          // We have to continue with this parent later.
          $process_parents[] = $parent;
          // Use the current term as parent for the next iteration.
          $process_parents[] = $term->tid;

          // Reset pointers for child lists because we step in there more often
          // with multi parents.
          reset($children[$vid][$term->tid]);
          // Move pointer so that we get the correct term the next time.
          next($children[$vid][$parent]);
          break;
        }
      } while ($child = next($children[$vid][$parent]));

      if (!$has_children) {
        // We processed all terms in this hierarchy-level, reset pointer
        // so that this function works the next time it gets called.
        reset($children[$vid][$parent]);
      }
    }
  }

  return $tree;
}
