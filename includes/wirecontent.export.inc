<?php

/**
 * Export last 10 published nodes from site to concatenation db
 */
function etype_wire_node_export() {

  /* check connection */
  if (_etype_wire_check_connection() === false) {
    watchdog('etype_wire', "No entry in settings.php for wire database", [], WATCHDOG_ERROR);
    return;
  }

  $sections = [];

  /* Get settings */
  $dbHandle = _etype_wire_connect();
  $sql = "SELECT `data` FROM `settings`";
  $stmt = $dbHandle->prepare($sql);
  $stmt->execute();
  $stmt->setFetchMode(PDO::FETCH_OBJ);
  $res = $stmt->fetchAll();
  $data = unserialize($res[0]->data);
  $order = 'n.' . variable_get('etype_wire_export_order', 'created');
  $number = variable_get('etype_wire_export_number', '10');
  $days = variable_get('etype_wire_export_since', '10');
  $type = variable_get('etype_wire_content_type', 'article');
  if (!empty($data['sections'])) {
    $sections = explode("|", $data['sections']);
  }
  if (empty(($data['taxonomy']))) {
    $data['taxonomy'] = 'field_section';
  }

  /* get last n published/updated nodes from this site */
  $query = db_select('node', 'n');
  $query->distinct();
  $query->join('field_data_' .  $data['taxonomy'], 'f', 'n.nid
    = f.entity_id');
  $query->join('taxonomy_term_data', 't', 'f.' .  $data['taxonomy'] . '_tid = t.tid');
  $query->fields('n');
  $query->condition('n.type', $type);
  $query->condition('n.status', '1');
  if (count($sections) > 0) {
    $db_or = db_or();
    foreach ($sections as $item) {
      $db_or->condition('t.name', trim($item), 'REGEXP');
    }
    $query->condition($db_or);
  }
  $query->orderBy($order, 'DESC');
 if ($days > 0) {
    $date_diff = strtotime("-$days days");
    $query->condition($order, $date_diff, '>');
  } else if ($number > 0) {
    $query->range(0, $number);
  }
  $results = $query->execute();

  $nodes = [];
  while ($n = $results->fetchAssoc()) {
    $nodes[] = $n;
  }

  if (count($nodes) > 0) {

    $dbHandle = _etype_wire_connect();
    $text_setting = variable_get('etype_wire_text_format', 'html');

    global $base_url;
    $site_name = variable_get('site_name', 'Drupal');
    $etype_wire_cluster = variable_get('etype_wire_cluster');
    $cluster = implode($etype_wire_cluster, ',');

    /* Delete old nodes */
    $sql = "DELETE FROM node WHERE `site_name` = ?";
    $stmt = $dbHandle->prepare($sql);
    $stmt->execute([$site_name]);

    foreach ($nodes as $node) {
      $object = node_load($node['nid']);
      $wrapper = entity_metadata_wrapper('node', $object);

      $body = $wrapper->body->value();
      switch ($text_setting) {
        case 'plain':
          $safe_body = $body['value'];
          break;

        default;
          $safe_body = $body['safe_value'];
      }

      if (isset ($wrapper->field_kicker)) {
        $kicker = $wrapper->field_kicker->value();
        $safe_kicker = strip_tags($kicker['safe_value']);
      } else {
        $safe_kicker = '';
      }

      $image_uri = isset($wrapper->field_image[0]->value()['uri']) ? $wrapper->field_image[0]->value()['uri'] : '';
      if (!empty($image_uri)) {
        // create real url
        $file_path = file_create_url($image_uri);
        // get caption
        $caption = isset($wrapper->field_image[0]->value()['image_field_caption']['value']) ? $wrapper->field_image[0]->value()['image_field_caption']['value'] : '';
        $safe_caption = strip_tags($caption);
      }
      else {
        $file_path = '';
        $safe_caption = '';
      }

      /* Sites without UUID */
      if (!isset ($node['uuid'])) {
        $node['uuid'] = '';
      }

      $sql = "INSERT INTO node (
`nid`, `vid`, `type`, `language`, `title`, `body`, `file`, `kicker`, `caption`, `uid`, `status`, `created`, `changed`, `comment`, `promote`, `sticky`, `tnid`, `translate`, `uuid`, `site`, `site_name`, `cluster`)
VALUES (
    '" . $node['nid'] . "',
    '" . $node['vid'] . "',
     'article',
     '" . $node['language'] . "',
      " . $dbHandle->quote($node['title']) . ",
       " . $dbHandle->quote($safe_body) . ",
        '" . $file_path . "',
         '" . $safe_kicker . "',
          " . $dbHandle->quote($safe_caption) . ",
           '" . $node['uid'] . "',
            '0',
             '" . $node['created'] . "',
              '" . $node['changed'] . "',
               '" . $node['comment'] . "',
                '0',
                 '0',
                  '" . $node['tnid'] . "',
                   '" . $node['translate'] . "',
                    '" . $node['uuid'] . "',
                     ?,
                      ?,
                       ?)";

      $dbHandle->setAttribute( PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION );
      $stmt = $dbHandle->prepare($sql);
      try {
        $stmt->execute(["$base_url", "$site_name", "$cluster"]);
        // echo 'Error occurred:'.implode(":",$dbHandle->errorInfo());
      } catch (Exception $e) {
        print_r($stmt->errorInfo());
        $error = $stmt->errorInfo();
        watchdog('etype_wire', "There was an error inserting the data for $base_url.", [], WATCHDOG_ERROR);
        die ("Error: (".$error[0].':'.$error[1].') '.$error[2]);
      }
    }

  }

}
