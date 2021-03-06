<?php

/**
 * @file
 * Functions to add Wire content.
 */
require_once('Encoding.php');

use \ForceUTF8\Encoding;  // It's namespaced now.

function etype_wire_node_add($nid, $site_name) {

  /* some basic error testing */
  if (!isset($nid)) {
    drupal_set_message(t('Unable to continue, no node id available'), 'error');
    drupal_goto('admin/content/wirecontent');
    return;
  }

  /* connect to wire db and get desired node */
  $dbHandle = _etype_wire_connect();

  function get_all($dbHandle, $nid, $site_name) {
    $fixed_site_name = urldecode($site_name);
    $sql = "SELECT * FROM `wire_node` WHERE `nid` = $nid AND `site_name` = \"$fixed_site_name\"";
    $stmt = $dbHandle->prepare($sql);
    $stmt->execute();
    $stmt->setFetchMode(PDO::FETCH_OBJ);
    $result = $stmt->fetchAll();
    return $result;
  }

  $nodes = get_all($dbHandle, $nid, $site_name);

  if (count($nodes) == 0) {
    drupal_set_message(t('Unable to continue, no nodes available'), 'error');
    drupal_goto('admin/content/wirecontent');
    return;
  }

  $old_node = $nodes[0];

  global $user;
  $entity_type = 'node';

  $type = variable_get('etype_wire_content_type', 'article');
  $new_node = entity_create($entity_type, [
    'type' => $type,
    'uid' => $user->uid,
    'status' => 0,
  ]);

  $entity = entity_metadata_wrapper($entity_type, $new_node);
  $entity->title->set($old_node->title);

  /* body formatting */
  $new_body = $old_node->body;
  $new_body = Encoding::toUTF8($new_body);
  $entity->body->set(['value' => $new_body]);

  $entity->language(LANGUAGE_NONE);

  if (!empty($old_node->file)) {

    $old_file_path = $old_node->file;

    // system_retrieve_file does not set path correctly, so needs to be set explicitly
    $arr = explode('files/', $old_file_path);
    $path = 'public://' . $arr[1];
    $file = system_retrieve_file($old_file_path, $path, TRUE, FILE_EXISTS_RENAME);
    $file->display = '1';
    $file->description = "";
    $file->image_field_caption = [
      'value' => $old_node->caption,
      'format' => 'plain_text',
    ];
    $file = file_save($file);
    try {
      if (!$entity->field_image->set([(array) $file])) {
        throw new Exception("Wrapper not saved");
      }
    } catch(Exception $e) {
      watchdog('etype_wire', "Could not save image.", $variables = [], $severity = WATCHDOG_NOTICE);
    }

  } else {
    $entity->field_image->set([]);
  }

  $entity->save(TRUE);
  $new_nid = $entity->getIdentifier();

  // record saved node
  global $base_url;
  $sql = "REPLACE INTO node_site SET `original_nid` = $nid, `real_nid` = $new_nid, `site_base` = '$base_url' ";
  $stmt = $dbHandle->prepare($sql);
  $stmt->execute();

  drupal_set_message(t('Node added.'));
  drupal_goto('node/' . $new_nid . '/edit');

}